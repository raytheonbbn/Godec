#include "PythonComponent.h"
#include "godec/ComponentGraph.h"
#ifndef ANDROID
#define PY_ARRAY_UNIQUE_SYMBOL PYTHON_COMPONENT_ARRAY_API
#include <numpy/arrayobject.h>
#endif
#include <codecvt>
#ifndef _MSC_VER
#include <dlfcn.h>
#else
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif
#include <functional>

namespace Godec {

LoopProcessor* PythonComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new PythonComponent(id, configPt);
}
std::string PythonComponent::describeThyself() {
    return "Calls a Python script";
}

/* PythonComponent::ExtendedDescription
Just like the Java component, this allows for calling an arbitrary Python script specified in "script_file_name" (omit the .py ending and any preceding path, those go into "python_path")  and doing some processing inside it. The Python script needs to define a class with the "class_name" name, and the component will instantiate an instance with the "class_constructor_param" string as the only parameter. "python_executable" points to the Python executable to use, "python_path" to the "PYTHONPATH" values to set so Python finds all dependent libraries.

The input is a dict with the specified input streams as key, and the value whatever the message is (currently only FeatureDecoderMessage type is implemented, which is are Numpy matrices), the output (i.e. the return value) should be in the same format.

Look at `test/python_test.json` for an example.
*/

#ifndef ANDROID
int PythonComponent_init_numpy() {
    import_array();
}
#endif

PythonComponent::PythonComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {

#ifndef ANDROID
    // Initialize the Python interpreter.
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::string tmpPythonExec = configPt->get<std::string>("python_executable", "Python executable to use");
    mPythonExec = converter.from_bytes(tmpPythonExec);
    Py_SetProgramName((wchar_t*)mPythonExec.c_str());
    Py_Initialize();
    PythonComponent_init_numpy();

    std::string scriptName = configPt->get<std::string>("script_file_name", "Python script file name");
    boost::filesystem::path scriptPath(scriptName);
    std::string pythonPath = configPt->get<std::string>("python_path", "PYTHONPATH to set (cwd and script folder get added automatically)");
    boost::filesystem::path pythonPathPath(pythonPath);
    std::wstring cwd = boost::filesystem::current_path().wstring();
#ifdef _MSC_VER
    std::wstring completePythonPath = cwd+L";"+scriptPath.parent_path().wstring()+L";"+pythonPathPath.wstring();
#else
    std::wstring completePythonPath = cwd+L":"+scriptPath.parent_path().wstring()+L":"+pythonPathPath.wstring();
#endif
    PySys_SetPath((wchar_t*)completePythonPath.c_str());

    if (PyImport_ImportModule("numpy") == NULL) { PyErr_Print(); GODEC_ERR << "Unable to import numpy"; }
    if (PyImport_ImportModule("numpy.core.multiarray") == NULL) {PyErr_Print(); GODEC_ERR << "Unable to import numpy.core.multiarray";}

    mPModule = PyImport_ImportModule((char*)(scriptPath.stem().string().c_str()));

    if (mPModule == NULL) { PyErr_Print(); GODEC_ERR << getLPId(false) << "Could not load Python script '" << scriptName << "'. Make sure to omit the '.py' ending, and set the 'python_path' parameter if necessary (keep in mind to use the correct separator: ':' for Linux, ';' for Windows)";}

    mPModuleDict = PyModule_GetDict(mPModule);
    if (mPModuleDict == NULL) GODEC_ERR << getLPId(false) << "Could not extract Python module dict";

    std::string className = configPt->get<std::string>("class_name", "The name of the class that contains the ProcessMessage function");
    std::string constructorParam = configPt->get<std::string>("class_constructor_param", "string parameter passed into class constructor");
    PyObject* pClass = PyDict_GetItemString(mPModuleDict, className.c_str());
    if (pClass == NULL) GODEC_ERR << getLPId(false) <<  "Could not find class '" << className << "' in Python module";
    PyObject* pConstructorArgs = PyTuple_New(4);
    PyObject* pClassParam = PyUnicode_FromString(constructorParam.c_str());
    PyObject* pStdoutParam = PyFile_FromFd(STDOUT_FILENO, "<stdout>", "w", -1, NULL, NULL, "\n", 0);
    PyObject* pStderrParam = PyFile_FromFd(STDERR_FILENO, "<stderr>", "w", -1, NULL, NULL, "\n", 0);
    PyTuple_SetItem(pConstructorArgs, 0, pClassParam);
    PyTuple_SetItem(pConstructorArgs, 1, pStdoutParam);
    PyTuple_SetItem(pConstructorArgs, 2, pStderrParam);
    PyTuple_SetItem(pConstructorArgs, 3, isVerbose() ? Py_True : Py_False);
    mPClass = PyObject_CallObject(pClass, pConstructorArgs);
    if(mPClass == NULL) GODEC_ERR << getLPId(false) << ": Could not instantiate class!";

    std::string expectedInputs = configPt->get<std::string>("expected_inputs", "comma-separated list of expected input slots");
    std::string expectedOutputs = configPt->get<std::string>("expected_outputs", "comma-separated list of expected output slots");

    std::list<std::string> requiredInputSlots;
    boost::split(requiredInputSlots, expectedInputs,boost::is_any_of(","));
    for(auto it = requiredInputSlots.begin(); it != requiredInputSlots.end(); it++) {
        addInputSlotAndUUID(*it, UUID_AnyDecoderMessage); // GodecDocIgnore
        // addInputSlotAndUUID(<slots from 'expected_inputs'>, UUID_AnyDecoderMessage);  // Replacement for above godec doc ignore
    }

    std::list<std::string> requiredOutputSlots;
    boost::split(requiredOutputSlots, expectedOutputs,boost::is_any_of(","));
    requiredOutputSlots.erase(std::remove_if(requiredOutputSlots.begin(), requiredOutputSlots.end(), std::bind2nd(std::equal_to<std::string>(), "")), requiredOutputSlots.end());
    // .push_back(Slots From 'expected_outputs');  // godec doc won't catch the above construct
    initOutputs(requiredOutputSlots);
    mPMainThread = PyEval_SaveThread();
#else
    GODEC_ERR << "The Python component can not be used under Android";
#endif
}

PythonComponent::~PythonComponent() {
#ifndef ANDROID
    PyEval_RestoreThread(mPMainThread);
    Py_DECREF(mPClass);
    /* These are leaked references, but I'm assuming it's a tiny amount
        Py_DECREF(pClass);
        Py_DECREF(pConstructorArgs);
        Py_DECREF(pClassParam);
        Py_DECREF(pStdoutParam);
        Py_DECREF(pStderrParam); */
    Py_Finalize();
#endif
}

#ifndef ANDROID
PyObject* PythonComponent::DecoderMsgHashToPython(const DecoderMessageBlock& msgBlock) {
    const auto& map = msgBlock.getMap();
    PyObject* pMsgHash = PyDict_New();
    for(auto it = map.begin(); it != map.end(); it++) {
        auto nonConstMsg = boost::const_pointer_cast<DecoderMessage>(it->second);
        PyObject* pMsg = nonConstMsg->toPython();
        PyDict_SetItemString(pMsgHash, it->first.c_str(), pMsg);
        Py_DECREF(pMsg);
    }
    return pMsgHash;
}

unordered_map<std::string, DecoderMessage_ptr> PythonComponent::PythonDictToDecoderMsg(PyObject* pDict) {
    unordered_map<std::string, DecoderMessage_ptr> out;
    PyObject *pSlot, *pMsg;
    Py_ssize_t pos = 0;
    while (PyDict_Next(pDict, &pos, &pSlot, &pMsg)) {
        std::string slot = PyUnicode_AsUTF8(pSlot);
        DecoderMessage_ptr msg = GetComponentGraph()->PythonToDecoderMsg(pMsg);
        out[slot] = msg;
    }

    return out;
}
#endif

void PythonComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
#ifndef ANDROID
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject* pMsgBlock = DecoderMsgHashToPython(msgBlock);
    PyObject* pProcessMessage = PyUnicode_FromString("ProcessMessage");
    PyObject* pResult = PyObject_CallMethodObjArgs(mPClass, pProcessMessage, pMsgBlock, NULL);
    if(pResult == NULL) {
        PyErr_Print();
        GODEC_ERR << getLPId(false) << ": Could not call Python ProcessMessage function!";
    }
    auto retMsg = PythonDictToDecoderMsg(pResult);
    for(auto it = retMsg.begin(); it != retMsg.end(); it++) {
        pushToOutputs(it->first, it->second);
    }
    Py_DECREF(pMsgBlock);
    Py_DECREF(pProcessMessage);
    Py_DECREF(pResult);
    PyGILState_Release(gstate);
#endif
}

}
