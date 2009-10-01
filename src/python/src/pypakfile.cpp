// TODO: This is *very* crude for now, need to figure out some neat way for
// 	 python to deal with C++ streams ...
#include <istream>
#include <fstream>
#include <sstream>

#include "eastwood/StdDef.h"
#include "eastwood/Exception.h"

#include "pyeastwood.h"
#include "pypakfile.h"


using namespace eastwood;

static int
PakFile_init(Py_PakFile *self, PyObject *args)
{
    Py_ssize_t size;
    char *fileName = NULL;
    if (!PyArg_ParseTuple(args, "s#", &fileName, &size))
	return -1;

    self->stream = new std::ifstream(fileName, std::ios::in | std::ios::binary);
    if(!self->stream->good()) {
	PyErr_SetFromErrno(PyExc_IOError);
	return -1;
    }

    self->pakFile = new PakFile(*self->stream);

    return 0;
}

static PyObject *
PakFile_alloc(PyTypeObject *type, Py_ssize_t nitems)
{
    Py_PakFile *self = (Py_PakFile *)PyType_GenericAlloc(type, nitems);
    self->pakFile = NULL;
    self->stream = NULL;
    self->fileSize = -1;
    self->mode = MODE_CLOSED;

    return (PyObject *)self;
}

static void
PakFile_dealloc(Py_PakFile *self)
{
    if(self->pakFile)
    	delete self->pakFile;
    if(self->stream) {
	self->stream->close();
	delete self->stream;
    }
    PyObject_Del((PyObject*)self);
}

static PyObject *
PakFile_listfiles(Py_PakFile *self)
{
    PyObject *fileList = PyTuple_New(self->pakFile->entries());
    for(uint32_t i = 0; i < self->pakFile->entries(); i++)
	PyTuple_SetItem(fileList, i, PyString_FromString(self->pakFile->getFileName(i).c_str()));
    return fileList;
}

static PyObject *
PakFile_open(Py_PakFile *self, PyObject *args)
{
    Py_ssize_t size;
    char *fileName = NULL;
    if (!PyArg_ParseTuple(args, "s#", &fileName, &size))
	return NULL;

    try {
    	self->pakFile->open(fileName);
    } catch(FileException e) {
	PyErr_Format(PyExc_IOError, "%s: %s", e.getLocation().c_str(), e.getMessage().c_str());
	return NULL;
    }

    self->fileSize = self->pakFile->size();
    self->mode = MODE_READ;
    Py_RETURN_TRUE;
}

static PyObject *
PakFile_close(Py_PakFile *self)
{
    self->pakFile->close();
    self->fileSize = -1;
    self->mode = MODE_CLOSED;
    Py_RETURN_TRUE;
}

static PyObject *
PakFile_read(Py_PakFile *self, PyObject *args)
{
	std::streamoff	offset = static_cast<std::streamoff>(self->pakFile->tellg());
	size_t bytesrequested = -1,
	       left = self->fileSize - offset;
	PyObject *v;

	if (!PyArg_ParseTuple(args, "|l:read", &bytesrequested))
		return NULL;

	if (bytesrequested > left)
		bytesrequested = left;
	if (bytesrequested > UINT_MAX) {
		PyErr_SetString(PyExc_OverflowError,
	"requested number of bytes is more than a Python string can hold");
		return NULL;
	}
	v = PyString_FromStringAndSize(NULL, bytesrequested);
	if (v == NULL)
		return NULL;
	self->pakFile->read(PyString_AS_STRING(v), bytesrequested);
	if(static_cast<std::streamoff>(self->pakFile->tellg())  == self->fileSize-1)
	    self->mode = MODE_READ_EOF;

	return v;
}

static PyObject *
PakFile_seek(Py_PakFile *self, PyObject *args)
{
    int where = 0;
    PyObject *offobj;
    std::streamoff offset;
    PyObject *ret = NULL;

    if (!PyArg_ParseTuple(args, "O|i:seek", &offobj, &where))
	return NULL;

    switch (self->mode) {
	case MODE_READ:
	    self->pakFile->seekg(where, std::ios::beg);
	    self->pakFile->seekg(PyInt_AsLong(offobj), std::ios::cur);
	    offset = static_cast<std::streamoff>(self->pakFile->tellg());
	    if(static_cast<std::streamoff>(self->pakFile->tellg())  == self->fileSize-1)
		self->mode = MODE_READ_EOF;

	case MODE_READ_EOF:
	    break;

	case MODE_CLOSED:
	    PyErr_SetString(PyExc_ValueError,
		    "I/O operation on closed file");
	    goto cleanup;

	default:
	    PyErr_SetString(PyExc_IOError,
		    "seek works only while reading");
	    goto cleanup;
    }	

    Py_INCREF(Py_None);
    ret = Py_None;

cleanup:
    return ret;
}

static PyMethodDef PakFile_methods[] = {
    {"listfiles", (PyCFunction)PakFile_listfiles, METH_NOARGS, NULL},
    {"open", (PyCFunction)PakFile_open, METH_VARARGS, NULL},
    {"close", (PyCFunction)PakFile_close, METH_NOARGS, NULL},
    {"read", (PyCFunction)PakFile_read, METH_VARARGS, NULL},
    {"seek", (PyCFunction)PakFile_seek, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}		/* sentinel */
};


PyTypeObject PakFile_Type = {
    PyObject_HEAD_INIT(NULL)
    0,						/*ob_size*/
    "pyeastwood.PakFile",			/*tp_name*/
    sizeof(Py_PakFile),				/*tp_basicsize*/
    0,						/*tp_itemsize*/
    (destructor)PakFile_dealloc,		/*tp_dealloc*/
    0,						/*tp_print*/
    0,						/*tp_getattr*/
    0,						/*tp_setattr*/
    0,						/*tp_compare*/
    0,						/*tp_repr*/
    0,						/*tp_as_number*/
    0,						/*tp_as_sequence*/
    0,						/*tp_as_mapping*/
    0,						/*tp_hash*/
    0,						/*tp_call*/
    0,						/*tp_str*/
    PyObject_GenericGetAttr,			/*tp_getattro*/
    PyObject_GenericSetAttr,			/*tp_setattro*/
    0,						/*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/*tp_flags*/
    0,						/*tp_doc*/
    0,						/*tp_traverse*/
    0,						/*tp_clear*/
    0,						/*tp_richcompare*/
    0,						/*tp_weaklistoffset*/
    0,						/*tp_iter*/
    0,						/*tp_iternext*/
    PakFile_methods,				/*tp_methods*/
    0,						/*tp_members*/
    0,						/*tp_getset*/
    0,                      			/*tp_base*/
    0,                      			/*tp_dict*/
    0,                      			/*tp_descr_get*/
    0,                      			/*tp_descr_set*/
    0,                      			/*tp_dictoffset*/
    (initproc)PakFile_init,			/*tp_init*/
    PakFile_alloc,	    			/*tp_alloc*/
    PyType_GenericNew,	      			/*tp_new*/
    0,		          			/*tp_free*/
    0,                      			/*tp_is_gc*/
    0,						/*tp_bases*/
    0,						/*tp_mro*/
    0,						/*tp_cache*/
    0,						/*tp_subclasses*/
    0,						/*tp_weaklist*/
    0,						/*tp_del*/
    0						/*tp_version_tag*/
};
