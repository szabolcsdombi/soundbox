#include <Windows.h>

#include <Python.h>
#include <structmember.h>

#define QOA_NO_STDIO
#define QOA_IMPLEMENTATION
#include "qoa.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_FAST_SCALED_FLOAT
#define STB_VORBIS_MAX_CHANNELS 2
#include "stb_vorbis.h"

static PyObject * meth_qoa_decode(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"data", NULL};
    PyObject * qoa_data;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", keywords, &qoa_data)) {
        return NULL;
    }
    if (!PyBytes_Check(qoa_data)) {
        PyErr_SetString(PyExc_TypeError, "data must be bytes");
        return NULL;
    }
    Py_ssize_t size = PyBytes_Size(qoa_data);
    const char * buffer = PyBytes_AsString(qoa_data);
    qoa_desc qoa = {};
    short * ptr = qoa_decode((unsigned char *)buffer, (int)size, &qoa);
    PyObject * data = PyBytes_FromStringAndSize((char *)ptr, qoa.channels * qoa.samples * sizeof(short));
    free(ptr);
    return Py_BuildValue("(Nii)", data, qoa.samplerate, qoa.channels);
}

static PyObject * meth_qoa_encode(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"data", "frequency", "channels", NULL};
    PyObject * data;
    int frequency, channels;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Oii", keywords, &data, &frequency, &channels)) {
        return NULL;
    }
    if (!PyBytes_Check(data)) {
        PyErr_SetString(PyExc_TypeError, "data must be bytes");
        return NULL;
    }
    const char * buffer = PyBytes_AsString(data);
    Py_ssize_t samples = PyBytes_Size(data) / sizeof(short);
    qoa_desc qoa = {(unsigned)channels, (unsigned)frequency, (unsigned)samples};
    unsigned int size = 0;
    void * ptr = qoa_encode((short *)buffer, &qoa, &size);
    PyObject * res = PyBytes_FromStringAndSize((char *)ptr, size);
    free(ptr);
    return res;
}

static PyObject * meth_ogg_decode(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"data", NULL};
    PyObject * ogg_data;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", keywords, &ogg_data)) {
        return NULL;
    }
    if (!PyBytes_Check(ogg_data)) {
        PyErr_SetString(PyExc_TypeError, "data must be bytes");
        return NULL;
    }
    Py_ssize_t buffer_size = PyBytes_Size(ogg_data);
    const char * buffer = PyBytes_AsString(ogg_data);
    int channels, frequency;
    short * output = NULL;
    int size = stb_vorbis_decode_memory((unsigned char *)buffer, (int)buffer_size, &channels, &frequency, &output);
    if (size <= 0) {
        PyErr_SetString(PyExc_RuntimeError, "failed to decode ogg data");
        return NULL;
    }
    PyObject * data = PyBytes_FromStringAndSize((char *)output, channels * frequency * sizeof(short));
    free(output);
    return Py_BuildValue("(Nii)", data, frequency, channels);
}

struct ALCdevice;
struct ALCcontext;

#define AL_PITCH 0x1003
#define AL_POSITION 0x1004
#define AL_DIRECTION 0x1005
#define AL_VELOCITY 0x1006
#define AL_LOOPING 0x1007
#define AL_BUFFER 0x1009
#define AL_GAIN 0x100A
#define AL_ORIENTATION 0x100F

static ALCdevice * device;
static ALCcontext * context;

ALCcontext * (* alcCreateContext)(ALCdevice * device, const int * attrlist);
char (* alcMakeContextCurrent)(ALCcontext * context);
ALCdevice * (* alcOpenDevice)(const char * devicename);
char (* alcCloseDevice)(ALCdevice * device);

void (* alListenerfv)(int param, const float * values);
void (* alListeneriv)(int param, const int * values);
void (* alGenSources)(int n, int * sources);
void (* alDeleteSources)(int n, const int * sources);
void (* alSourcefv)(int source, int param, const float * values);
void (* alSourceiv)(int source, int param, const int * values);
void (* alSourcePlayv)(int n, const int * sources);
void (* alSourceStopv)(int n, const int * sources);
void (* alSourceRewindv)(int n, const int * sources);
void (* alSourcePausev)(int n, const int * sources);
void (* alSourceQueueBuffers)(int source, int nb, const int * buffers);
void (* alSourceUnqueueBuffers)(int source, int nb, int * buffers);
void (* alGenBuffers)(int n, int * buffers);
void (* alDeleteBuffers)(int n, const int * buffers);
void (* alBufferData)(int buffer, int format, const void * data, int size, int samplerate);
void (* alBufferfv)(int buffer, int param, const float * values);
void (* alBufferiv)(int buffer, int param, const int * values);

static PyObject * meth_init(PyObject * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"dll_path", NULL};

    const char * dll_path = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", (char **)keywords, &dll_path)) {
        return NULL;
    }

    HMODULE openal = LoadLibraryA(dll_path);
    if (!openal) {
        PyErr_BadInternalCall();
        return NULL;
    }

    *(PROC *)&alcCreateContext = GetProcAddress(openal, "alcCreateContext");
    *(PROC *)&alcMakeContextCurrent = GetProcAddress(openal, "alcMakeContextCurrent");
    *(PROC *)&alcOpenDevice = GetProcAddress(openal, "alcOpenDevice");
    *(PROC *)&alcCloseDevice = GetProcAddress(openal, "alcCloseDevice");

    *(PROC *)&alListenerfv = GetProcAddress(openal, "alListenerfv");
    *(PROC *)&alListeneriv = GetProcAddress(openal, "alListeneriv");
    *(PROC *)&alGenSources = GetProcAddress(openal, "alGenSources");
    *(PROC *)&alDeleteSources = GetProcAddress(openal, "alDeleteSources");
    *(PROC *)&alSourcefv = GetProcAddress(openal, "alSourcefv");
    *(PROC *)&alSourceiv = GetProcAddress(openal, "alSourceiv");
    *(PROC *)&alSourcePlayv = GetProcAddress(openal, "alSourcePlayv");
    *(PROC *)&alSourceStopv = GetProcAddress(openal, "alSourceStopv");
    *(PROC *)&alSourceRewindv = GetProcAddress(openal, "alSourceRewindv");
    *(PROC *)&alSourcePausev = GetProcAddress(openal, "alSourcePausev");
    *(PROC *)&alSourceQueueBuffers = GetProcAddress(openal, "alSourceQueueBuffers");
    *(PROC *)&alSourceUnqueueBuffers = GetProcAddress(openal, "alSourceUnqueueBuffers");
    *(PROC *)&alGenBuffers = GetProcAddress(openal, "alGenBuffers");
    *(PROC *)&alDeleteBuffers = GetProcAddress(openal, "alDeleteBuffers");
    *(PROC *)&alBufferData = GetProcAddress(openal, "alBufferData");
    *(PROC *)&alBufferfv = GetProcAddress(openal, "alBufferfv");
    *(PROC *)&alBufferiv = GetProcAddress(openal, "alBufferiv");

    device = alcOpenDevice(NULL);
    if (!device) {
        PyErr_BadInternalCall();
        return NULL;
    }

    context = alcCreateContext(device, NULL);
    if (!context) {
        PyErr_BadInternalCall();
        return NULL;
    }

    alcMakeContextCurrent(context);
    Py_RETURN_NONE;
}

static PyMethodDef module_methods[] = {
    {"qoa_decode", (PyCFunction)meth_qoa_decode, METH_VARARGS | METH_KEYWORDS},
    {"qoa_encode", (PyCFunction)meth_qoa_encode, METH_VARARGS | METH_KEYWORDS},
    {"ogg_decode", (PyCFunction)meth_ogg_decode, METH_VARARGS | METH_KEYWORDS},
    {"init", (PyCFunction)meth_init, METH_VARARGS | METH_KEYWORDS},
    {},
};

static void module_free(void * module) {
    if (device) {
        alcCloseDevice(device);
        device = NULL;
    }
}

static PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT, "soundbox", NULL, -1, module_methods, NULL, NULL, NULL, (freefunc)module_free,
};

extern "C" PyObject * PyInit_soundbox() {
    PyObject * module = PyModule_Create(&module_def);
    return module;
}
