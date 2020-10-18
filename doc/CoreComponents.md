# Godec core component list

[AudioPreProcessor](#audiopreprocessor)  
[Average](#average)  
[Energy](#energy)  
[FeatureMerger](#featuremerger)  
[FeatureNormalizer](#featurenormalizer)  
[FileFeeder](#filefeeder)  
[FileWriter](#filewriter)  
[Java](#java)  
[MatrixApply](#matrixapply)  
[Merger](#merger)  
[NoiseAdd](#noiseadd)  
[Python](#python)  
[Router](#router)  
[SoundcardPlayer](#soundcardplayer)  
[SoundcardRecorder](#soundcardrecorder)  
[Submodule](#submodule)  
[Subsample](#subsample)  


## AudioPreProcessor

---

### Short description:
Component that takes audio and does the following operations on it: Zero-mean, Pre-emphasis, resampling.

### Extended description:
This component supports both BinaryDecoderMessage as well as AudioDecoderMessage input in its "stream_audio" slot. The output slots are enumerated in the form of "`streamed_audio_0`", "`streamed_audio_1`" etc, up to the number specified in "`max_out_channels`"  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| max\_out\_channels | int | maximum number of output channels that need to be defined |
| output\_scale | float | Scale the output audio by this factor |
| preemphasis\_factor | float | Pre-emphasis factor (float, no-op value=1.0) |
| target\_sampling\_rate | float | If required, resample audio to this rate |
| zero\_mean | bool | Zero-mean incoming waveform (true, false) |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| streamed\_audio | AudioDecoderMessage,BinaryDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| audio\_info | 


## Average

---

### Short description:
Averages the incoming feature stream. Will issue the averaged features when seeing end-of-utterance

#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| apply\_log | bool | Doing summation in log domain (true, false) |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| features | FeaturesDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| features | 


## Energy

---

### Short description:
Calculates energy features on incoming windowed audio (feature input message produced by Window component)

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| windowed\_audio | FeaturesDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| features | 


## FeatureMerger

---

### Short description:
Combines incoming feature and audio streams into one stream. Specify each incoming stream in the 'inputs' list with feature\_stream\_0, feature\_stream\_1 etc. The features will be merged in that order.

#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| num\_streams | int | Number of incoming streams to merge |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| feature\_stream\_[0-9] | FeaturesDecoderMessage,AudioDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| features | 


## FeatureNormalizer

---

### Short description:
Normalizes a feature stream with various algorithms (covariance, diagonal etc)

### Extended description:
This normalization component supports 4 different types of "normalization":  
  
"covariance:" This builds up a proper covariance matrix (full, or only diagonal) and normalizes the features according to it. The normalization can be selectively be means normalization ("norm_means") and vars, i.e. covariance normalization ("norm_vars"). "decay_num_frames" introduces a "memory decay" through a simple exponential decay  
  
"L2": Normalize each feature column vector with its L2 norm  
  
"n_frame_average": Maybe not really "normalization", but it combines n frames (from "update_stats_every_n_frames") into one averaged output vector. So, it produces one output vector every n input vectors. "zero_pad_input_to_n_frames" specifies if for the case of to little data at the very end (i.e. < N frames), whether to zero-pad it to N frames, or rather averages over the frames we have.  
  
"log_limiter": Not really quite sure what it does, it seems to normalize between max and min with some scaling factor. Look at the source code for more information.  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| covariance\_type | string | Full or only diagonal covariance normalization (full, diagonal) |
| decay\_num\_frames | int64\_t | Frame window size for 1/e dropoff of memory (set <= 0 for infinite memory) |
| feature\_size | int | incoming features size |
| norm\_means | bool | Normalize means |
| norm\_vars | bool | Normalize variances |
| normalization\_type | string | Normalization type (covariance, log\_limiter, L2, n\_frame\_average) |
| processing\_mode | string | Whether to process in batch or low-latency mode (Batch, LowLatency) |
| update\_stats\_every\_n\_frames | int64\_t | In LowLatency mode, with what frequency to update the statistics. Lower value = lower latency, but also more CPU. |
| zero\_pad\_input\_to\_n\_frames | bool | Whether to zero-pad the input matrix to have the size of 'update\_stats\_every\_n\_frames' |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| features | FeaturesDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| features | 


## FileFeeder

---

### Short description:
Component that feeds file content into the network. Can read analist, text, JSON and Numpy npz features

### Extended description:
This is the component for providing your Godec graph with data for processing. The different feeding formats in details:  
  
"analist": A text file describing line-by-line segments in audio file (support formats: WAV and NIST_1A, i.e. SPHERE). The general format is "\<wave file base name without extension\> -c \<channel number, 1-based\> -t \<audio format\> -f \<sample start\>-\<sample end\> -o \<utterance ID\> -spkr \<speaker ID\>" . "wave_dir" parameter specifies the direction of where to find the wave files, "wave_extension" the file extension. The "feed_realtime_factor" specifies how much faster than realtime it should feed the audio (higher value = faster)  
  
"text": A simple line-by-line file with text in it. The component will feed one line at a time, as a BinaryDecoderMessage with timestamps according to how many words were in the line  
  
"json": A single file containing a JSON array of items. Each item will be pushed as a JsonDecoderMessage  
  
"numpy_npz": A Python Numpy npz file. The parameter "keys_list_file" specifies a file which contains a line-by-line list of npz+key combo, e.g. "my_feats.npz:a", which would extract key "a" from my_feats.npz. "feature_chunk_size" sets the size of the feature chunks to be pushed  
  
The "control_type" parameter describes whether the FileFeeder will just work off one single configuration on startup, i.e. batch processing ("single_on_startup"), or whether it should receive these configurations via an external slot ("external") that pushes the exact same component JSON configuration from the outside via the Java API. The latter is essentially for "dynamic batch processing" where the FileFeeder gets pointed to new data dynamically.  
  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| audio\_chunk\_size | int | Audio size in samples for each chunk |
| control\_type | string | Where this FileFeeder gets its source data from: single-shot feeding on startup ('single\_on\_startup'), or as JSON input from an input stream ('external') |
| feature\_chunk\_size | int | Size in frames of each pushed chunk |
| feed\_realtime\_factor | float | Controls how fast the audio is pushed. A value of 1.0 simulates soundcard reading of audio (i.e. pushing a 1-second chunk takes 1 second), a higher value pushes faster. Use 100000 for batch pushing |
| input\_file | string | Input file |
| keys\_list\_file | string | File containing a list (line by line) of keys that are contained in the npz file and are then fed in that order |
| source\_type | string | File type of source file (analist, text, numpy\_npz, json) |
| time\_upsample\_factor | int | Factor by which the internal time stamps are increased. This is to prevent multiple subunits having the same time stamp. |
| wave\_dir | string | Audio waves directory |
| wave\_extension | string | Wave file extension |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| control | JsonDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| conversation\_state | 
| output\_stream | 


## FileWriter

---

### Short description:
Component that writes stream to file

### Extended description:
The writing equivalent to the FileFeeder component, for saving output. Available "input_type":  
  
"audio": For writing AudioDecoderMessage messages. "output_file_prefix" specifies the path prefix that each utterance gets written to. The incoming audio are float values expected to be normalied to -1.0/1.0 range  
  
"raw_text": BinaryDecoderMessage expected that gets converted into text and written into "output_file"  
  
"json": Expects JsonDecoderMessage as input, concatenates the JSONs into the "output_file"  
  
"features": FeatureDecoderMessage as input, output file is a Numpy NPZ file, with the utterance IDs as keys  
  
Like the FileFeeder, the "control_type" specifies whether this is a one-shot run that goes straight off the JSON parameters ("single_on_startup"), or whether it receives these JSON parameters through an external channel ("external"). When in the external mode, it also requires the ConversationState stream from the FileFeeder that produced the content  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| control\_type | string | Where this FileWriter gets its output configuration from: single-shot on startup ('single\_on\_startup'), or as JSON input from an input stream ('external') |
| input\_type | string | Input stream type (audio, raw\_text, features, json) |
| json\_output\_format | string | Output format for json (raw\_json, ctm, fst\_search, mt) |
| npz\_file | string | Output Numpy npz file name |
| output\_file | string | Json output file path |
| output\_file\_prefix | string | Output audio file path prefix |
| sample\_depth | int | wave file sample depth (8,16,32) |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| control | JsonDecoderMessage|
| file\_feeder\_conversation\_state | ConversationStateDecoderMessage|
| input\_stream | AnyDecoderMessage|
| streamed\_audio | AudioDecoderMessage|



## Java

---

### Short description:
Loads an arbitrary JAR, allocates a class object and calls ProcessMessage() on it.

### Extended description:
This component can load any Java code inside a JAR file. It expects a class to be defined ("class_name") and will instantiate the class constructor with a String that contains the "class_constructor_param" parameter (set this to whatever you need to initialize your instance in a specific manner). The parameters "expected_inputs" and "expected_outputs" configure the input and output streams of this component, and in turn also what gets passed into the the Java ProcessMessage() function.  
  
The signature of ProcessMessage is  
  
`HashMap<String,DecoderMessage> ProcessMessage(HashMap<String, DecoderMessage>)`  
  
The input HashMap's keys correspond to the input streams, the output is expected to contain the necessary output streams of the component.  
  
See `java/com/bbn/godec/regression/TextTransform.java` and `test/jni_test.json` for an example.  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| class\_constructor\_param | string | string parameter passed into constructor |
| class\_name | string | Full-qualified class name, e.g. java/util/ArrayList |
| expected\_inputs | string | comma-separated list of expected input slots |
| expected\_outputs | string | comma-separated list of expected output slots |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| <slots from 'expected\_inputs'> | AnyDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| Slots From 'expected\_outputs' | 


## MatrixApply

---

### Short description:
Applies a matrix (from a stream) to an incoming feature stream

#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| augment\_features | bool | Whether to add a row of 1.0 elements to the bottom of the incoming feature vector (to enable all affine transforms) |
| matrix\_npy | string | matrix file name. Matrix should be plain Numpy 'npy' format |
| matrix\_source | string | Where the matrix comes from: From a file ('file') or pushed in through an input stream ('stream') |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| features | FeaturesDecoderMessage|
| matrix | MatrixDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| transformed\_features | 


## Merger

---

### Short description:
Merges streams that were created by Router. Streams are specified as input\_stream\_0, input\_stream\_1 etc, up to \"num\_streams\". Time map is the one created by the Router

### Extended description:
Refer to the Router extended description to a more detailed description. This component expects all conversation states from the upstream Router, as well as all the processed streams  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| num\_streams | int | Number of streams that are merged |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| conversation\_state\_[0-9] | ConverstionStateDecoderMessage|
| input\_streams\_[0-9] | AnyDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| output\_stream | 


## NoiseAdd

---

### Short description:
Adds noise to audio stream

#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| noise\_scaling\_factor | float | Scale factor, in relation to incoming audio, of noise. 1.0 = 0dB noise, lower is quieter |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| streamed\_audio | AudioDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| streamed\_audio | 


## Python

---

### Short description:
Calls a Python script

### Extended description:
Just like the Java component, this allows for calling an arbitrary Python script specified in "script_file_name" (omit the .py ending and any preceding path, those go into "python_path")  and doing some processing inside it. The Python script needs to define a class with the "class_name" name, and the component will instantiate an instance with the "class_constructor_param" string as the only parameter. "python_executable" points to the Python executable to use, "python_path" to the "PYTHONPATH" values to set so Python finds all dependent libraries.  
  
The input is a dict with the specified input streams as key, and the value whatever the message is (currently only FeatureDecoderMessage type is implemented, which is are Numpy matrices), the output (i.e. the return value) should be in the same format.  
  
Look at `test/python_test.json` for an example.  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| class\_constructor\_param | string | string parameter passed into class constructor |
| class\_name | string | The name of the class that contains the ProcessMessage function |
| expected\_inputs | string | comma-separated list of expected input slots |
| expected\_outputs | string | comma-separated list of expected output slots |
| python\_executable | string | Python executable to use |
| python\_path | string | PYTHONPATH to set (cwd and script folder get added automatically) |
| script\_file\_name | string | Python script file name |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| <slots from 'expected\_inputs'> | AnyDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| Slots From 'expected\_outputs' | 


## Router

---

### Short description:
Splits an incoming stream into separate streams, based on the binary decision of another input stream

### Extended description:
The router component can be used to split streams across several branches (e.g. if you have a language detector upstream and want to direct the streams to the language-specific component). The mechanism is very easy, the Router create N output conversation state message streams, each of which has the "ignore_data" set to true or false, depending on the routing at that point. The branches' components can check this flag and do some optimization (while still emitting empty messages that account for the stream time). So, all branch components see all the data. The Merger component can be used downstream to merge the output together (it chooses the right results from each stream according to those conversation state messages.  
  
There are two modes the Router decides where to route:  
  
"sad_nbest": "routing_stream" is expected to be an NbestDecoderMessage, where the 0-th nbest entry is expected to contain a sequence of 0 (nonspeech) or 1 (speech), which the router will use to route the stream  
  
"utterance_round_robin": Simple round robin on an utterance-by-utterance basis  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| num\_outputs | int | Number of outputs to distribute to |
| router\_type | string | Type of routing. Valid values: 'sad\_nbest', 'utterance\_round\_robin' |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| routing\_stream | AnyDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| Slot: conversation\_state\_[0-9] | 


## SoundcardPlayer

---

### Short description:
Opens sounds card, plays incoming audio data

#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| num\_channels | int | Number of channels (1=mono, 2=stereo) |
| sample\_depth | int | Sample depth (8,16,24 etc bit) |
| sampling\_rate | float | Sampling rate |
| soundcard\_identifier | string | Soundcard identifier on Linux, use 'aplay -L' for list |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| streamed\_audio | AudioDecoderMessage|



## SoundcardRecorder

---

### Short description:
Opens sounds card, records audio and pushes it as audio stream

### Extended description:
The "start_on_boot" parameter, when set to "false", will require the "control" stream to be specified. That stream expects a ConversationStateDecoderMessage. If the stream is within an utterance (according to the ConversationState) the soundcard will output recorded audio, if it sees the end-of-utterance it stops, and so on.  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| chunk\_size | int | Chunk size in samples (set to -1 for minimum latency configuratio) |
| num\_channels | int | Number of channels (1=mono, 2=stereo) |
| sample\_depth | int | Sample depth (8,16,24 etc bit) |
| sampling\_rate | float | Sampling rate |
| soundcard\_identifier | string | Soundcard identifier on Linux, use 'arecord -L' for list |
| start\_on\_boot | bool | Whether audio should be pushed immediately |
| time\_upsample\_factor | int | Factor by which the internal time stamps are increased. This is to prevent multiple subunits having the same time stamp. |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| control | ConversationStateDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| conversation\_state | 
| streamed\_audio | 


## Submodule

---

### Short description:
The Godec equivalent of a 'function call'

### Extended description:
This component allows for a "Godec within a Godec". You specify the JSON file for that graph with the "file" parameter, and the entire sub-graph gets treated as a single component. Great for building reusable libraries.  
  
For a detailed discussion, [see here](UsingGodec.md), section "Submodules".  
  


#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| file | string | The json for this subnetwork |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| name of slot inside sub-network that will be injected | AnyDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| Slot: name of stream inside sub-network to be pulled out | 


## Subsample

---

### Short description:
Godec equivalent of subsample-feats executable, for subsampling/repeating features

#### Parameters
| Parameter | Type | Description |
| --- | --- | --- |
| num\_skip\_frames | int | Only emit every <num\_skip\_frames> frames. Set to negative to also repeat the emitted frame <num\_skip\_frames> (resulting in the same number of total frames) |

#### Inputs
| Input slot | Message Type | 
| --- | --- | 
| features | FeaturesDecoderMessage|

#### Outputs
| Output slot | 
| --- | 
| features | 
