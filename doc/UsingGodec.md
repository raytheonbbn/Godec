[Back](../README.md)  

# Using Godec
-----------------

- [Running Godec](#Running-Godec)  
- [The JSON](#The-JSON)  
- [Parameters](#Parameters)  
- [Overriding Parameters](#Overriding-parameters)  
- [The "global_opts" section](#The-global_opts-section)  
- [Connecting the components](#Connecting-the-components)  
- [Submodules](#Submodules)  
- [Available components](#Available-components)  
- [Interacting with Godec via the API](#Interacting-with-Godec-via-the-API)  

## Running Godec

Godec comes in two major forms: As a command line executable `godec`, and as a shared library (`libgodec.[so|dll]`)

Running command-line Godec is as simple as

> `godec myfile.json`

To set up the required environment variables to run it on Linux, an installation of Godec contains an "env.sh" script. Source it, and you are able to run Godec with its core components.

Running Godec this way assumes something inside the graph is feeding input source data, i.e. a *FileFeeder* component, and writing the results into file(s) with a *FileWriter* component.  Godec will start up, process all input, and eventually shut down. 
In contrast, when Godec is run as a library, the assumption is that the data is being passed in from the outside via the API, and the results pulled out the same way.

## The JSON

Here is a simplified JSON file to illustrate the basic setup:

    "file_feeder":
    {
      "verbose": "false",
      "type": "core:FileFeeder",
      "source_type": "analist",
      "outputs":
      {
        "output_stream": "raw_audio",
        "conversation_state": "convstate_input"
      },
      "inputs": {}
    },
    "preprocess":
    {
      "verbose": "false",
      "type": "core:AudioPreProcessor",
      "target_sampling_rate": "8000", // Here is a comment
      "inputs":
      {
        "streamed_audio": "raw_audio",
        "conversation_state": "convstate_input"
      },
      "outputs":
      {
        "streamed_audio_0" : "preproc_audio"
      }
    },
       ...

Each component has a unique name ("file_feeder", "preprocess") that is is identified by, and a type ("core:FileFeeder, "core:AudioPreProcessor"). The "core:" prefix here means that these components are part of the core components list, and will be accessed by loading libgodec_**core**.so. To use a component from another library, say the "tflite" component provided by libgodec_**tensorflow**.so, you would use "tensorflow:tflite" as the type descriptor. In general, Godec tries to load a libgodec_[typename].so for a given type prefix.

> Note:  The "core:" prefix can be left off, it is implied when it is missing

The JSON parser in Godec is extended to understand C-style commenting, i.e. // and /* */ pairs.

To deactivate an entire component, there are two options: Entirely comment it out with a /* */ pair, or prefix the compoent name with a "#" (e.g. "#file_feeder"). This will make Godec ignore the component.

### Parameters

Each component has a set of parameters you have to specify, a list of the parameters for the core components can be [found here](CoreComponents.md). 

Godec parameters are **strict and parsimonious**, which means

* There are no default parameters that can be omitted, every necessary parameter has to exist in the JSON

* One can not have superfluous parameters, i.e. one either has to remove a superfluous parameter, or prefix it with "#"
 to park it 

---
**NOTE**
This approach has been met with some criticism, and it is worth pointing out the motivation behind it: In our experience, default/optional parameters cause the following issues:

1. Unknown parameters and functionality. Usually parameter files get inherited and copied from project to project, and once a parameter is no longer specified, the downstream users are unaware of it and it will become a "forgotten feature"

2. Dead code paths. Once functionality has dropped out of the collective knowledge of the users, source code maintainers find themselves having to deal with code paths they do not understand and are thus reluctant to change or remove. Eventually the code becomes unmaintainable.

---

If you forget to specify a parameter, you will be greeted with an error similar to this:

<span style="color:red">
  ################################################################################   

  ERROR  
  Toplevel.resample: Parameter 'zero_mean' not specified  
  Description: Zero-mean incoming waveform (true, false)  
  ################################################################################ 

</span>

As mentioned, if you still would like to keep the parameter around in the JSON, you can either comment it out, or "park" the parameter by prefixing it with a "#", e.g. "#extra_parm".

With the above being said, there **are** a few optional component parameters:

- "verbose" (default false): When set to true for a component, the component will dump incoming and outgoing messages. 

- "log_file": This will redirect the logging output to the specified file.

### Overriding parameters

Often in experiments, one needs to run a decoding 100 times in parallel, with each run having a slightly set of input audio files (as an example). In order to not have to create copies of the JSON file, godec allows overriding JSON parameters from both the command line, and the API.

For example, to override the `input_file` parameter of a `feeder` component, you would say

> godec -x "feeder.input_file=run_9.analist" mygraph.json

Note that the parameter has to exist to override it (this is done to catch typo errors). If you want to create a new parameter, prefix it with "!":

> godec -x "feeder.!input_file=run_9.wav"

This circumvents the typo check, so use sparingly.

You can also override a parameter that is deeply nested inside a Submodule (see below for more about Submodules). by going through its `override` section. For example, assuming you have a Submodule called "featex" in your top-level JSON and you want set the MFCC component to verbose inside the Submodule, you can do 

> godec -x "featex.!override.mfcc.!verbose=true"

(note again how we here force the creation of both `override` and `verbose`. If both of those are already specified inside the JSON and you are just changing the values, the "!" is not needed)

### The global_opts section

Another good way of making your JSON file more general is to use variables. They are defined in the special "global_opts" section:

	"global_opts":
	{
	  "MODELS_DIR": "fillmein"
	},
	"decoder":
	{
		"config": "$(MODELS_DIR)/config.txt"
		"am_source": "$(MODELS_DIR)/am_source.txt"
		..."
 
 With a JSON file like this, you can then change all file locations in one swoop with a simple

 > godec -x "global_opts.MODELS_DIR=/new/location"
 
Variables are inherited down into SubModules, with any local global_opts declaration overriding the parent one.

Connecting the components
--------------------------------
**Inputs and outputs**

Components are connected to each other by their "inputs" and "outputs" sections, e.g.

	"decoder":
	{
		...
		"inputs":
		{
			"features": "ivec_feats"			
		},		
		"outputs":		
		{
			"nbest": "dec_nbest"
		}

Each component has a set of mandatory inputs and outputs (just like other parameters, these can change depending on configuration).

The general format of input/output lines is
> "slot": "stream_ID"

The "slot" is specific to the component (e,g, "features", "nbest"), whereas the "stream_ID" is an ID (e.g. "ivec_featrs", "dec_nbest") that uniquely identifies this message stream. The simple rule is, **output sections define stream IDs, and input sections connect to them**. One or more components can connect to the same stream, and will all receive the same data, this reduces both CPU and memory overhead. Godec will complain if you didn't connect all inputs, or tried to define the same stream ID twice. You can however leave stream ID unconnected (e.g. if a component produces multiple outputs but you only care about one of them).

## Submodules

An important component is the *SubModule*, as it allows you to use another JSON graph as if it was just another component. This allows for creating useful "libraries" of components you can just plug in if you need it, without cluttering the main JSON, or even the necessity of understanding what happens internally inside that subgraph. Note that even if you pull in the same JSON twice as submodules, they are entirely independent.

For the *SubModule*, the inputs/outputs sections are special. They define the bridge between the parent-level streams and the sub-level streams. A small example: 

	"MySubModule":
	{
		"type": "SubModule",
		"input_file": "my_sub.json",
		"inputs":
		{
			"submodule_audio": "parent_audio"			
		},		
		"outputs":		
		{
			"submodule_feats": "parent_feats"
		}

As a simple rule for the inputs/outputs section, **the right refers to streams in the parent-level, the left to streams in the sub-level**. In this specific example, the Submodule will pass the parent-level "parent_audio" stream and pass it into the sub-graph as a stream called "submodule_audio". In the reverse, it will pass the "submodule_feats" stream from the subgraph into the parent-level under the name "parent_feats". Note that there is no clash between the stream IDs of the parent-level and the sub-level, their stream naming is entirely local.

The corresponding subgraph JSON would be:

	"my_featex":
	{
		...
		"inputs":
		{
			"audio": "submodule_audio"			
		},		
		"outputs":		
		{
			"feats": "submodule_feats"
		}
 
It should be noted, one can not run this sub-graph JSON on its own since it refers to stream IDs that are not defined inside it (the ones passed in by the parent-level Submodule). However, this fact actually leads to the following good-practice suggestion:


---
**Good practice**

During offline experimentation, it is tempting to just plug the FileFeeder and FileWriter components (i.e. the data sources and data sinks) into the same-level JSON as the processing components. The downside of this approach is that when it comes to the production hand-off, this JSON can not be taken as-is and plugged into an engine that loads Godec through its API. It would instantiate the FileWriter/FileFeeder components and probably fail.

Because of this, it is **highly** suggested to follow the practice of always having the processing JSON be a Submodule JSON. This way, if you want to run it in the offline experiment, you just create a wrapper, top-level JSON that has the FileFeeder/FileWriter components in it, and simply pulls in the processing JSON as a Submodule. When it comes to production handoff, this subgraph JSON can be taken exactly as-is and plugged into the API.

---

## Available components

Hopefully a component library comes with its own extensive (or autogenerated [like this](CoreComponents.md)) documentation about the components it contains, but to get a quick glance at the core components for example, type 

> godec list core

or more generically,

> godec list <library name>

(the same library name as you would use inside the JSON to specify it).

## Interacting with Godec via the API

This is a [separate page](../java/README.md)
