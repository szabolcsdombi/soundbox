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

static PyMethodDef module_methods[] = {
    {"qoa_decode", (PyCFunction)meth_qoa_decode, METH_VARARGS | METH_KEYWORDS},
    {"qoa_encode", (PyCFunction)meth_qoa_encode, METH_VARARGS | METH_KEYWORDS},
    {"ogg_decode", (PyCFunction)meth_ogg_decode, METH_VARARGS | METH_KEYWORDS},
    {},
};

static PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "soundbox", NULL, -1, module_methods};

extern "C" PyObject * PyInit_soundbox() {
    PyObject * module = PyModule_Create(&module_def);
    return module;
}
