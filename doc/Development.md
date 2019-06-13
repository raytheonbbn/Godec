[Back](../README.md)  

# Developing new components and messages

If you are here without having read the section "[The inner workings of Godec](Details.md)", read that section first, it is key to understanding some of concepts involved here.

[Adding a new component](#adding-a-new-component)
* [Constructor](#constructor)
* [Start](#start)
* [ProcessMessage](#processmessage)
* [pushToOutputs](#pushtooutputs)
* [describeThyself](#describethyself)
* [Shutdown](#shutdown)

[Adding new messages](#adding-new-messages)
* [getUUID](#getuuid)
* [describeThyself](#describethyself-1)
* [create](#create)
* [clone](#clone)
* [mergeWith](#mergewith)
* [canSliceAt](#cansliceat)
* [sliceOut](#sliceout)
* [JNI,Python transfer](#jni-python-transfer)

## Adding a new component

As a general rule, when developing a new component, have a look at the [core components](CoreComponents.md) (or other Godec component libraries) and identify one that is closest to your desired new functionality to base your code on. 

All components inherit from the "LoopProcessor" class, which requires you to define the following functions:

### Constructor

The constructor for the component itself has no restrictions, but since the `LoopProcessor` base needs to be initialized, it needs to at least take the `id` (the name of the component as defined in the JSON) and `configPt` arguments (the class containing the parameters etc of the component as defined in the JSON).

```c++
MyComponent::MyComponent(std::string id, ComponentGraphConfig* configPt) :
  // Initialize the underlying LoopProcessor
  LoopProcessor(id,configPt)
{
```

Aside from the component-specific initialization, three things need to be done:

##### Defining input slots 

For each input slot+message type combination, the `addInputSlotAndUUID` function needs to be called:

```c++
    addInputSlotAndUUID(SlotWindowedAudio, UUID_FeaturesDecoderMessage);
```

The first argument that declares the slot name is just a std::string and can by anything (especially if your component has some input streams with unique identifiers, e.g. "covariance_matrix"), but to encourage consistency among components, a few slot names are predefined in  [ChannelMessenger.h](../src/include/godec/ChannelMessenger.h).

`addInputSlotAndUUID` can be called multiple times for the same slot if this slot can accept more than one type of message (e.g. the `AudioPreProcessor` component can take both an `AudioDecoderMessage` or `FeatureDecoderMessage` as the audio stream input), and for the rare case that a component can deal with any type of message, `UUID_AnyDecoderMessage` can be used. Use this last UUID only in cases that really warrant it, as it deactivates all the internal checks about message type consistency.
    

##### Defining output slots 

```c++
  std::list<std::string> outputSlots;
  outputSlots.push_back(SlotMatrix);
  initOutputs(outputSlots);
```

This list defines the outputs the component will push its results to, and it needs to match the later `pushToOutputs` calls in the `ProcessMesssage` function. If a component code later tries to push to an output that wasn't defined via `initOutputs`, Godec will exit with an error message.



##### Extracting JSON parameters

The way to extract any parameter from the JSON is via `configPt->get<T>()`, with `<T>` being any type the Boost `lexical_cast` function supports.

```c++
  float addValue = configPt->get<float>("add_value", "Value to add to input");
```

The first argument is the JSON parameter name, the second argument is the **documentation** of that parameter. If a Godec user leaves out this parameter, Godec will exit and tell the user that the parameter is missing, along with the description of what the parameter is about. 

Now, the following might be contentious, but do yourself a favor and only extract parameters in the code paths they make sense for and when they are needed. For example, avoid this type of code:

```c++
  bool shouldWeUseMatrix = configPt->get<bool>("should_we_use_matrix", "Should we use a matrix");
  std::string matrixFilename = configPt->get<std::string>("matrix_filename", "Matrix filename for adding");
```

The result of the above code is that the JSON **has** to look like this:

```c++
  "should_we_use_matrix": "false", // Let's not use a matrix here
  "matrix_filename": "", // Um, we are not using a matrix, so I guess empty string is ok here? 
```

Instead, use code like this

```c++
  bool shouldWeUseMatrix = configPt->get<bool>("should_we_use_matrix", "Should we use a matrix");
  if (shouldWeUseMatrix) {
    std::string matrixFilename = configPt->get<std::string>("matrix_filename", "Matrix filename for adding");
```

The difference here is, if a user sets `shouldWeUseMatrix` to `false`, he does not need to specify the `matrix_filename` parameter (as it makes no sense to do so in this configuration!). Conversely, specifying the parameter in this confguration will throw an error as it is a parameter not needed by the code. The point of the exercise here is to avoid "parameter rot" that is often an issue for complex engines. The originator of the component code might know that certain parameters are not used in certain configurations, but intern XYZ a year later does not know this, and the engine telling him/her "this parameter makes no sense in this configuration" is of immense help. Keep in mind that prefixing a parameter with "#" (e.g. `#matrix_filename`") makes Godec ignore it, which allows you the "park" parameters in the JSON, but it is immediately obvious they are not used.

### Start

The `Start()` function gets called after the entire Godec network has been stood up, and all components have this function called. It has a default empty implementation, but you can override it if you need to start code that relies on the output connections to be available (e.g. a soundcard feeder).

### ProcessMessage

**`ProcessMessage(const DecoderMessageBlock& msgBlock)`**

The details on when Godec (well the `LoopProcessor` class to be exact) calls this function is covered in "[The inner workings of Godec](Details.md)".

The `DecoderMessageBlock` class contains the messages that were sliced out, and they are accessed with the `get<T>` function. The slots used here need to match the specified input slots in the constructor.

```c++
auto audioMsg = msgBlock.get<AudioDecoderMessage>(SlotAudio);
```

NOTE: What you receive is actually a Boost shared pointer to a message. The actual message inside is declared `const`, and you should never, ever override this `const` declaration and modify the contents of that message. The reason is, other components might work off the exact message and you'd be pulling the rug under them if you modify the contents of the message.

The internals of the message are accessed like this 

```c++
  Vector audioSnippets = audioMsg->mAudio;
```

Note that Godec internally uses the Eigen library for vectors and matrices. Eigen suggests to keep thing in "column-major" for performance reasons, which means that for example a feature matrix has each feature vector as a **column**. For some libraries (e.g. Kaldi) this sometimes means you have to transpose the data to make it row-major.

### pushToOutputs

When the code has processed the input and has something to be pushed to an output slot, first create a message via its static creator function:

```c++
  auto matrixMsg = MatrixDecoderMessage::create(getLPId(), inputMsg->getTime(), myOutputMatrix); 
```

Every message type has its own specific creator function signature, but at least for the predefined core messages the first two arguments are 1. the "tag" (use getLPId(), which contains the component name) and 2. the message's end timestamp. If your code just pushes out a result that accounts for the same time span as the input, just use one of the input messages' `getTime()` function.

Pushing the message to an output is then just
            
```c++
  pushToOutputs(SlotMatrix, matrixMsg);
```

As mentioned elsewhere, the timestamps used for the messages that are pushed out are incredibly important and are hands-down the biggest source for errors. 

A few general rules:

- The time stream starts at 0. That means for example, an audio message with 16000 samples in it will have time stamp 15999, not 16000. 
- Absolutely all time needs to be accounted for. If your component has nothing of worth to push out, you need to decide whether you just want to wait until you have something to push out (which would mean that message would account for that combined time chunk), or you want to push an empty message.
- The index of the next message is expected to be +1 of the end of the previous message. While this is not explicitly enforced (messages don't have a start time member), violating the agreement can get you into trouble.
- Off-by-one errors are vicious. For example, if you find yourself having to calculate time stamps through ratios ("20% of this message, so 20% of time accounted for"), you have to be very careful calculating the output timestamps. The basic rule is, "timestamp of chunk = timestamps of next chunk -1". So, a 100-tick-span message chunked into 5 chunks has for its 3rd (i.e. zero-based index 2) chunk the timestamp "(100/5)*(2+1) - 1 = 59".

If you get messages like

- "Received out-of-order messages in slot..." or
- "We should not slice past the first message!"

it is likely due to incorrect timestamps. In order to debug, set the your component's verbose parameter to "true" and have a look what it gets in and what it pushes out.

### describeThyself

`describeThyself()` returns a string that describes the component. Please fill this in, it gets displayed with "godec list".

### Shutdown

 You only need to override `Shutdown()` if you have specific teardown to do at the end. If you override it, do not forget to call LoopProcess::Shutdown() in it!



## Adding new messages
---

It is very tempting to want to add a new message that fits exactly your needs. **However, first try to reuse an existing message type**. They have seen extensive debugging, and many of the core components work with them. As an example, at some point we thought about adding a "windowed audio" message (when audio has been cut into overlapping windows). However, it became clear that a stream of windowed audio is nothing but a stream of floating-point features really, each feature column being one piece of windowed audio.

To quickly enumerate the existing messages (defined in [GodecMessages.h](../src/core_components/GodecMessages.h)):

- **BinaryDecoderMessage** : The most generic of them all. Contains a vector of unsigned char that can hold any data, and a format string that describes the data. The receiving component is expected to know how to interpret the data based on the format
- **AudioDecoderMessage** : Contains float-value audio samples, along with sample rate, VTL stretch and ticks-per-sample information (ticks-per-sample meaning how much the timestamp gets increased for each sample)
- **FeaturesDecoderMessage**: Really useful for all kinds of purposes. Contains a column-major matrix (column-major means each column is a feature vector), as well as timestamps for the columns
- **MatrixDecoderMessage**: A generic matrix inside
- **NbestDecoderMessage**: Suprisingly useful also. Use this message for any ranked list of time-aligned entries. 
- **JsonDecoderMessage**: Contains a string with a valid JSON inside. Useful for nested and specific data structures, but obviously requires parsing upon consumption



So, if none of these match and you decide to implement a new functions, just as with a new component, try to identify an existing core message that is closest to the one you will create, and base your code on that. The key virtual functions to be defined are the following:

### getUUID

##### `uuid getUUID()`

##### `static uuid getUUIDStatic()`

These two functions uniquely identify your message type throughout the Godec network. Simply fetch a new UUID from https://www.uuidgenerator.net/  and return that `uuid` in those functions.

### describeThyself

##### `std::string describeThyself()`

This function gets called when a component's `verbose` flag is set to true and it outputs the incoming and outgoing messages. The returned string should summarize the message succinctly, but also be specific about its contents (e.g. the `MatrixDecoderMessage` will calculate the sum of the matrix's elements and show that). Since this function does not get called during normal runs (you don't want to spam your stdout), this function does not need to be particularly efficient.

### create

##### `DecoderMessage_ptr create(std::string tag, uint64_t time, ....)`

This is the static creator function for instantiating the message. You should keep the first two argument as-is, the following arguments are up to you. Use those two arguments to set the time and tag:

```c++
MatrixDecoderMessage* msg = new MatrixDecoderMessage();
msg->setTag(_tag);
msg->setTime(_time);
...
return DecoderMessage_ptr(msg);
```

### clone

###### `DecoderMessage_ptr clone()`

The `clone()` function should return an **deep** copy of the message, i.e. no memory-shared elements between the originating message and its clone.


---

The following three functions are part of the "slicing mechanism". To understand what they do, first read the ["The inner workings of Godec"](Details.md) page.

### mergeWith

##### `bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr& remainingMsg, bool verbose)`

The function gets called when a new message has arrived at a component and gets lined up with the other streams (cf. the figure in  ["Slicing and dicing"](Details.md#Slicing-and-dicing)) . If possible we try to merge messages of the same type together since this will result in a larger block being sliced out eventually. 

So, the question is, can the `this` message and the passed in `msg` be merged into one bigger message that does not lose crucial information that way? For example, an `AudioDecoderMessage` can be merged together with another one if and only if a) the sample rates are the same b) the number of ticks per sample is the same and c) the "descriptor string" matches too.

The `remainingMsg` is what remains of `msg` after merging. Additionally, the return value of the function is "is  `remainingMsg` populated?". So, as a quick runthrough of the different cases:

1. `msg` gets entirely merged: Update `this` members with the merged contents, don't do anything to `remainingMsg` and return `false` (since nothing is in `remainingMsg`)
2. `msg` can not be merged: Set `remainingMsg` to `msg`, return `true` (since `remainingMsg` is populated)
3. (Rare case) only part of `msg` was merged in: Update `this` members accordingly, set `remainingMsg` to the remainder and return `true`

### canSliceAt

##### `bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose)`

Can this message be sliced at `sliceTime`? Try to be as restrictive as you can, this will help uncover bugs in component code. For example, even the `AudioDecoderMessage` only returns `true` if the `sliceTime` is an integer multiple of its ticks-per-sample member.

The `msgList` and `streamStartOffset` are auxiliary information that can help making that decision.

Note, if `sliceTime` is equal to `getTime()` (equivalent to "give me the entire message") you should always return `true`.



### sliceOut

##### `bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose)`

All streams agree they can be sliced at `sliceTime`, so this is the function to do the slicing. Obviously, `canSliceAt and `sliceOut` need to agree on this ability.

`sliceMsg` gets set to the sliced-out part of the message, and `this` gets updated to the remainder. If it so happens that the entire message got asked for (`sliceTime == getTime()`), pop the first element (`msgList[0]`) from `msgList` and set `sliceMsg` to that (`msgList[0]` is the same as `this`);

The return value of this function is meaningless.

### JNI,Python transfer

###### `jobject toJNI(JNIEnv* env)`

###### `DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg)`

###### `PyObject* toPython()`

###### `DecoderMessage_ptr fromPython(PyObject* pMsg)`

These functions only need to be implemented if you are planning to a) have the message be shuttled from or to Java, or b) you are intending to use it in conjunction with the `Python` component. It is suggested to at least do a dummy implementation that results in a `GODEC_ERR`  when called.



## Compiling, linking

All of the above code needs to be compiled into a shared library (.dll for Windows, .so for Linux) and a few functions need to be exposed by the shared library so that Godec can load the components inside it. Use [godec_core.cpp](../src/core_components/godec_core.cpp) and [GodecMessages.cpp](../src/core_components/GodecMessages.cpp) as an example.
