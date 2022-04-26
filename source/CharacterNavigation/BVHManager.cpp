#include "BVHManager.hpp"
#include <Python.h>
#include <stdexcept>

namespace Mona{

    BVH_file::BVH_file(std::string &filePath, std::vector<std::string> jointNames){
        Py_Initialize();   // initialize Python
        PyInit_cython_interface();
        PyObject* listObj = PyList_New(jointNames.size());
        if (!listObj) throw new std::exception("Unable to allocate memory for Python list");
        for (unsigned int i = 0; i < jointNames.size(); i++) {
            PyObject* name = PyUnicode_FromString(jointNames[i].data());
            if (!name) {
                Py_DECREF(listObj);
                throw new std::exception("Unable to allocate memory for Python list");
            }
            PyList_SET_ITEM(listObj, i, name);
        }
        PyObject* pyFile = createFileObject();
        initFileInterface(pyFile, PyUnicode_FromString(filePath.data()), listObj);
        initFile(pyFile);
        Py_Finalize();
    }

    BVH_file::BVH_file(std::string& filePath) {
        Py_Initialize();   // initialize Python
        PyInit_cython_interface();
        PyObject* pyFile = createFileObject();
        initFileInterface(PyUnicode_FromString(filePath.data()), NULL);
        initFile(pyFile);
        Py_Finalize();
    }

    void BVH_file::initFile(PyObject* pyFile) {
        BVH_file_interface structFile = (BVH_file_interface)pyFile;
        m_jointNum = structFile.jointNum;
        m_frameNum = structFile.frameNum;
        m_frametime = structFile.frametime;

        // topology
        m_topology = new int[m_jointNum];
        if (PyList_Check(structFile.topology)) {
            for (Py_ssize_t i = 0; i < PyList_Size(structFile.topology); i++) {
                PyObject* value = PyList_GetItem(structFile.topology, i);
                m_topology[i] = (int)PyLong_AsLong(value);
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }

        // jointNames
        m_jointNames = new std::string[m_jointNum];
        if (PyList_Check(structFile.jointNames)) {
            for (Py_ssize_t i = 0; i < PyList_Size(structFile.jointNames); i++) {
                PyObject* name = PyList_GetItem(structFile.jointNames, i);
                m_jointNames[i] = PyUnicode_AsUTF8(name);
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }
        
        

        // offsets
        m_offsets = new float[m_jointNum][3];
        if (PyList_Check(structFile.offsets)) {
            for (Py_ssize_t i = 0; i < PyList_Size(structFile.offsets); i++) {
                PyObject* jointOff = PyList_GetItem(structFile.offsets, i);
                for (Py_ssize_t j = 0; j < PyList_Size(jointOff); j++) {
                    PyObject* valOff = PyList_GetItem(jointOff, j);
                    m_offsets[i][j] = (float)PyFloat_AsDouble(valOff);
                }

            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }

        // rotations
        m_rotations = new float[m_frameNum][m_jointNum][3];
        if (PyList_Check(structFile.rotations)) {
            for (Py_ssize_t i = 0; i < PyList_Size(structFile.rotations); i++) {
                PyObject* frameRot = PyList_GetItem(structFile.rotations, i);
                for (Py_ssize_t j = 0; j < PyList_Size(frameRot); j++) {
                    PyObject* jointRot = PyList_GetItem(frameRot, j);
                    for (Py_ssize_t k = 0; k < PyList_Size(jointRot); k++) {
                        PyObject* valRot = PyList_GetItem(jointRot, k);
                        m_rotations[i][j][k] = (float)PyFloat_AsDouble(valRot);
                    }
                }
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }


        // positions
        m_positions = new float[m_frameNum][m_jointNum][3];
        if (PyList_Check(structFile.positions)) {
            for (Py_ssize_t i = 0; i < PyList_Size(structFile.positions); i++) {
                PyObject* framePos = PyList_GetItem(structFile.positions, i);
                for (Py_ssize_t j = 0; j < PyList_Size(framePos); j++) {
                    PyObject* jointPos = PyList_GetItem(framePos, j);
                    for (Py_ssize_t k = 0; k < PyList_Size(jointPos); k++) {
                        PyObject* valPos = PyList_GetItem(jointPos, k);
                        m_positions[i][j][k] = (float)PyFloat_AsDouble(valPos);
                    }
                }
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }
    }

    BVH_writer::BVH_writer(std::string &staticDataPath){
        m_staticDataPath = staticDataPath;
    }

    void BVH_writer::write(float*** rotations, float** positions, float frametime, int frameNum, std::string& writePath){
        Py_Initialize();   // initialize Python
        PyInit_cython_interface();
        PyObject* pyWriter = createWriterObject();
        initWriterInterface(pyWriter, PyUnicode_FromString(writePath.data()));
        BVH_writer_interface structWriter = (BVH_writer_interface)pyWriter;
        int jointNum = structWriter.jointNum;
        // rotations
        PyObject* frameListRot = PyList_New( frameNum );
        for (unsigned int i = 0; i < frameNum; i++) {
            PyObject* jointListRot = PyList_New( jointNum );
            PyList_SET_ITEM(frameListRot, i, jointListRot);
            for (unsigned int j = 0; j < jointNum; j++) {
                PyObject* valListRot = PyList_New( 3 );
                PyList_SET_ITEM(jointListRot, j, valListRot);
                for (unsigned int k = 0; k < 3; k++) {
                    PyObject* valRot = PyFloat_FromDouble((double)rotations[i][j][k]);
                    PyList_SET_ITEM(valListRot, k, valRot);
                }
            }
        }


        // positions
        PyObject* frameListPos = PyList_New( frameNum );
        for (unsigned int i = 0; i < frameNum; i++) {
            PyObject* jointListPos = PyList_New( jointNum );
            PyList_SET_ITEM(frameListPos, i, jointListPos);
            for (unsigned int j = 0; j < jointNum; j++) {
                PyObject* valListPos = PyList_New( 3 );
                PyList_SET_ITEM(jointListPos, j, valListPos);
                for (unsigned int k = 0; k < 3; k++) {
                    PyObject* valPos = PyFloat_FromDouble((double)rotations[i][j][k]);
                    PyList_SET_ITEM(valListPos, k, valPos);
                }
            }
        }

        writeBVH_interface(pyWriter, frameListRot, frameListPos, PyUnicode_FromString(writePath.data()), PyFloat_FromDouble((double)frametime));
        Py_Finalize();
    }

}

