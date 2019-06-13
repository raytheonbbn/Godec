[Back](../README.md)


# Godec Java API

- [Introduction](#introduction)
- [Maven Build](#maven-build)
- [Access Godec from Java](#access-godec-from-java)
    - [Import Godec in maven project](#import-godec-in-maven-project)
    - [Specify push/pull endpoints](#specify-push/pull-endpoints)
    - [Push data into Godec](#push-data-into-godec)
    - [About Godec timestamp](#about-godec-timestamp)
    - [Pull result out of Godec](#pull-result-out-of-godec)
    - [Shutdown Godec](#shutdown-godec)

## Introduction
This directory contains Java JNI binding to the C++ Godec engine so that it can
be integrated into a Java project. The benefits of using Java API instead of
native C++ API are:

- Speed up frontend development: it is straightforward to integrate Godec into a server thanks to Java's rich server API.
 Managing threads, JSON object, deployment is also a lot easier in Java.
- Android support: Java API is the only way to run godec on Android.



## Maven build
It is possible to build the Java library using traditional `Makefile`. However,
the recommended way of using the godec Java binding is through maven.

One can build the project from command line:
```shell
$ cd godec/java
$ mvn clean package
```

To test that the java wrapper works correctly one can test the java version of godec binary:
```
godec/target/appassembler/bin/javaGodec <godec-config.json>
```
The binary takes the same JSON input as normal `godec` binary except the main program is written in Java and 
the native C++ engine is used for heavy lifting. 

**Note**: In order to successfully run Godec inside Java, both the ``java.library.path`` and Java ``classpath`` need to be set correctly. If Java can not locate any of the libraries, you will get an error similar to this one at runtime:

```java
Exception in thread "main" java.lang.UnsatisfiedLinkError: no godec in java.library.path
        at java.lang.ClassLoader.loadLibrary(ClassLoader.java:1867)
        at java.lang.Runtime.loadLibrary0(Runtime.java:870)
        at java.lang.System.loadLibrary(System.java:1122)
        at com.bbn.godec.Godec.<clinit>(Godec.java:31)
```

Just like for the regular command-line version, it is easiest to use the ``env.sh`` in the installation directory (wherever you told CMake to install the compiled Godec). The script defines the variables ``JAVA_LIBRARY_PATH`` and ``JAVA_CLASSPATH``, which can then be used the following way:
```
java -Djava.library.path=$JAVA_LIBRARY_PATH -cp "$JAVA_CLASSPATH" <further arguments>

To easily test whether the paths are set correctly, run it with an empty json. If it runs without errors, it shows the Java binding works correctly. You can now install the jar into your `~/.m2` folder:
```sh
$ mvn install
```

## Build with IntelliJ IDEA (optional)
*You can skip this section if you already installed the jars to your `~/.m2` folder from command line.*

The recommended Java IDE is IntelliJ IDEA. The steps to import and build the Java library are:
 
1. Download IntelliJ IDEA (Comminity version) from https://www.jetbrains.com/idea/ in order to import the
2. Launch the IDE
3. In the Welcome Dialog choose "Import Project"
4. Navigate to `gocec/java` directory and select `godec/java/pom.xml`
5. Hit "Next" buttons a few times to import the project
6. Once the project is loaded, choose "Build->Build Project" or hit Ctrl+F9 to build the project


## Access Godec from Java
## Import Godec in Maven project
There is no pre-built maven release for Godec Java API. One needs to first build and install the `SNAPSHOT` version of the 
Java API to `~/.m2` and import it as dependency in your project's `pom.xml` file:

```xml
<dependency>
  <groupId>com.bbn.bue</groupId>
  <artifactId>godec-all</artifactId>
  <version>1.0-SNAPSHOT</version>
</dependency>
```

After that, import Godec into your code:
```java
import com.bbn.godec.*;
import com.google.gson.*; // needed to manipulate JSON data returned from Godec
```

### Specify push/pull endpoints
Godec works by passing messages around different components. Once the engine starts, it keeps fetching messages from the system's
 **push endpoints**, processes them and then pushes the outcome messages to the **poll endpoints** as defined in the Godec config file.
Godec does not care if a message is pushed from C++ or from Java. The JNI binding handles message conversion between C++ and Java transparently.

Before we can initialize a Godec instance we need to specify push and pull endpoints. 

Let us define push endpoints:
```java
GodecPushEndpoints godecPushEndpoints = new GodecPushEndpoints();
godecPushEndpoints.addPushEndpoint("raw_audio_0");
godecPushEndpoints.addPushEndpoint("convstate_input_0");
godecPushEndpoints.addPushEndpoint("raw_audio_1");
godecPushEndpoints.addPushEndpoint("convstate_input_1");
```
Our system uses two streams. To distinguish push endpoints between streams suffix `_0` and `_1` are used. Those push endpoints must be defined in the Godec config file.

Next we specify pull endpoints:
```java
GodecPullEndpoints godecPullEndpoints = new GodecPullEndpoints();
// Godec support processing multiple streams in parallel 
// and eash pull endpoint must be attached to a PullStream first
// All pull endpoints within a pull stream are returned together.
// Here we specify ctm and convstate outputs for two independent STT streams. 
HashSet<String> pullStreams = new HashSet<String>();
pullStreams.add("ctm_output_0");
pullStreams.add("convstate_output_0");
godecPullEndpolints.addPullEndpoint("stt_stream_0", pullStreams);
pullStreams = new HashSet<String>();
pullStreams.add("ctm_output_1");
pullStreams.add("convstate_output_1");
godecPullEndpolints.addPullEndpoint("stt_stream_1", pullStreams);
```

There is also a global override list for overriding parameters globally.
```java
GodecJsonOverrides godecJsonOverrides = new GodecJsonOverrides();
// optionally add any overrides to godecJsonOverrides
```

Finally we can initialize the Godec engine.
```java
// Assume the godec config file is: godec-config.json
// The 5th boolean parameter (quiet) controls if the content of godec config will be printed to stdout during startup (false value).
Godec godec = new Godec("godec-config.json", godecJsonOverrides, godecPushEndpoints, godecPullEndpoints, false);
```

`godec` instance is now initialized. 

Note Godec uses some global variables internally for book-keeping. Until this is 
rectified, one can not run multiple `godec` instances simultaneously in the same process. The solution is to use multiple streams within one
Godec instance as shown in the code above. This is also more memory efficient since data structures can be shared between streams.

## Push data into Godec
Now that the core Godec engine is initialized and running, it waits on the *push endpoints* for incoming Godec messages.

Let us push some audio into push endpoints designated for the first stream:
```java
// format string for one 16bit PCM audio chunk at 8kHz sampling rate
// see sage/src/godec_sage/AudioPreProcessor-component.cc for a list of supported audio format
String formatString = "base_format=PCM;sample_width=16;sample_rate=8000;num_channels=1";
// audioData needs to be populated with raw audio bytes
byte[] audioData;
long timeStamp = 0;
DecoderMessage audioMsg = new BinaryDecoderMessage(
        "dummy", // this can be any string
        timeStamp, 
        audioData,
        ormatString);
godec.PushMessage("raw_audio_0", audioMsg);

bool lastChunkInUtt = false;   // last chunk in utt? Set to true to signal this is the last chunk of a utterance.
bool lastChunkInConvo = false; // last chunk in conversation? Set to true to signal this is the last chunk of a conversation.
DecoderMessage convMsg = new ConversationStateDecoderMessage(
        "dummy", // this can be any string
        timeStamp, 
        "utt_0", // utterance identifier
        lastChunkInUtt, 
        "convo_0",// conversation identifier
        lastChunkInConvo, 
            );
godec.PushMessage("convstate_input_0", convMsg);
```
We just pushed one audio chunk along with its matching conversational state message into Godec!

### About Godec timestamp
The timestamp `timeStamp` used in the above example can be any non-negative 64bit integer as long as it keeps increasing for 
each raw_audio/convostate_input pair pushed into the **same** stream. The timestamp is used by Godec internally to keep track
where things are within each stream. It does not have to have be `time`. It is perfectly fine to set each timestamp to be 
the number of audio samples pushed so far (`N`). A good practice is to set timestamp to be `K*N`, where `K` is an integer 
multipler greater than 1 such as `K=2`. The use of such multiple gives Godec the ability to *upsample* if needed. For example,
Godec will fail with an error if up sampling from `8000` Hz to `16000` Hz is performed in one of the components and Godec is 
fed with timestamps `0, 1, 2, 3, 4, 5...`. We can not have a timestamp of `0.5` for *up-sampled message* since the timestamp must be an integer. In this case
feeding Godec with timestamps `0, 2, 4, 6, 8, 10...` will work. Now the up-sampled messages will get timestamps: `0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10...`.

## Pull result out of Godec
As long as Godec engine is running, one can pull messages out of one of the pre-defined *pull endpoints*:

```java
final String pullEndpoint = "stt_stream_0";
final float timeOut = 0.5; // timeout in seconds.
// results will contain all available messages in stt_stream_0 at the time of the call.
// results will be null upon timeout.
final ArrayList<HashMap<String, DecoderMessage>> outputs = PullAllMessages(godec,
                    pullEndpoint, 
                    timeOut);
```

Note `PullAllMessages()` will raise `com.bbn.godec.ChannelClosedException` and 
return `null` if the stream is closed while
pulling messages.

Now we can go through the result list to extract output. In this case, we get CTM encoded 
as a [JsonDecoderMessage](http://e-gitlab.bbn.com/sage/sage/blob/master/doc/JsonDecoderMessages.md):
```java
for (final HashMap<String, DecoderMessage> result : results) {
    for (final Map.Entry<String, DecoderMessage> entry : result.entrySet()) {
        DecoderMessage decoderMessage = entry.getValue();
        if (entry.getKey().equals("ctm_output_0")) {
            // CTM output is a JsonDecoderMessage
            final JsonDecoderMessage msg = (JsonDecoderMessage)decoderMessage;
            final JsonArray words = msg.jsonObject.getAsJsonArray("words");
            for (int i = 0; i < words.size(); ++i) {
                final JsonObject word = words.get(i).getAsJsonObject();
                long beginTime = word.get("beginTime").getAsLong();
                long duration = word.get("duration").getAsLong();
                // begintime and duration are given in Godec timestamps
                // they need to be converted back to seconds in Java:
                // realTime = beginTime / timeTicksPerSecond
                String word = word.get("word").getAsString());
                float score = word.get("score").getAsFloat();
            }
        }
    }
}
```

Most Godec output messages are `JsonDecoderMessage`. Its data can be accessed via the `jsonObject` member variable, which
is an instance of `com.google.gson.JsonObject`. Please refer to the [gson project](https://github.com/google/gson) for usage.
 
## Shutdown Godec
```java
godec.BlockingShutdown(); // this will block until godec finishes processing all data.
```
