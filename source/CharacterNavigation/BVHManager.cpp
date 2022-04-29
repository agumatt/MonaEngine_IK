#include "BVHManager.hpp"
#include <stdexcept>
#include <algorithm>

namespace Mona{

    BVH_file::BVH_file(std::string filePath, std::vector<std::string> jointNames){
        if (PyImport_AppendInittab("cython_interface", PyInit_cython_interface) != 0) {
            fprintf(stderr, "Unable to extend Python inittab");
        }
        Py_Initialize();   // initialize Python
        std::string file_path = __FILE__;
        std::string dir_path = file_path.substr(0, file_path.rfind("\\"));
        std::replace(dir_path.begin(), dir_path.end(), '\\', '/');
        std::string src_path = "'" + dir_path + std::string("/bvh_python/pySrc") + "'";
        std::string pyLine = std::string("sys.path.append(") + src_path + std::string(")");
        PyRun_SimpleString("import sys");
        PyRun_SimpleString(pyLine.data());

        if (PyImport_ImportModule("cython_interface") == NULL) {
            fprintf(stderr, "Unable to import cython module.\n");
            if (PyErr_Occurred()) {
                PyErr_PrintEx(0);
            }
            else {
                fprintf(stderr, "Unknown error");
            }
        }
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
        BVH_file_interface* pyFilePtr = createFileInterface(PyUnicode_FromString(filePath.data()), listObj);
        initFile(pyFilePtr);
        Py_FinalizeEx();
    }

    BVH_file::BVH_file(std::string filePath) {
        if (PyImport_AppendInittab("cython_interface", PyInit_cython_interface) != 0) {
            fprintf(stderr, "Unable to extend Python inittab");
        }
        Py_Initialize();   // initialize Python
        std::string file_path = __FILE__;
        std::string dir_path = file_path.substr(0, file_path.rfind("\\"));
        std::replace(dir_path.begin(), dir_path.end(), '\\', '/');
        std::string src_path = "'" + dir_path + std::string("/bvh_python/pySrc") + "'";
        std::string pyLine = std::string("sys.path.append(") + src_path + std::string(")");
        PyRun_SimpleString("import sys");
        PyRun_SimpleString(pyLine.data());

        if (PyImport_ImportModule("cython_interface") == NULL) {
            fprintf(stderr, "Unable to import cython module.\n");
            if (PyErr_Occurred()) {
                PyErr_PrintEx(0);
            }
            else {
                fprintf(stderr, "Unknown error");
            }
        }


        BVH_file_interface* pyFilePtr = createFileInterface(PyUnicode_FromString(filePath.data()), PyBool_FromLong(0));

        BVH_writer_interface* pyWriterPtr = createWriterInterface(PyUnicode_FromString(filePath.data()));
        int jointNum = pyWriterPtr->jointNum;
        initFile(pyFilePtr);

        Py_FinalizeEx();
    }

    void BVH_file::initFile(BVH_file_interface* pyFile) {
        m_jointNum = pyFile->jointNum;
        m_frameNum = pyFile->frameNum;
        m_frametime = pyFile->frametime;

        int jointNum = m_jointNum;
        int frameNum = m_frameNum;

        // topology
        m_topology = new std::vector<int>(jointNum);
        if (PyList_Check(pyFile->topology)) {
            for (Py_ssize_t i = 0; i < PyList_Size(pyFile->topology); i++) {
                PyObject* value = PyList_GetItem(pyFile->topology, i);
                m_topology->push_back((int)PyLong_AsLong(value));
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }

        // jointNames
        m_jointNames = new std::vector<std::string>(jointNum);
        if (PyList_Check(pyFile->jointNames)) {
            for (Py_ssize_t i = 0; i < PyList_Size(pyFile->jointNames); i++) {
                PyObject* name = PyList_GetItem(pyFile->jointNames, i);
                m_jointNames->push_back(PyUnicode_AsUTF8(name));
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }
        
        

        // offsets
        m_offsets = new float*[jointNum];
        if (PyList_Check(pyFile->offsets)) {
            for (Py_ssize_t i = 0; i < PyList_Size(pyFile->offsets); i++) {
                PyObject* jointOff = PyList_GetItem(pyFile->offsets, i);
                m_offsets[i] = new float[3];
                for (Py_ssize_t j = 0; j < 3; j++) {
                    PyObject* valOff = PyList_GetItem(jointOff, j);
                    m_offsets[i][j] = (float)PyFloat_AsDouble(valOff);
                }

            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }

        // rotations
        m_rotations = new float**[frameNum];
        if (PyList_Check(pyFile->rotations)) {
            for (Py_ssize_t i = 0; i < PyList_Size(pyFile->rotations); i++) {
                PyObject* frameRot = PyList_GetItem(pyFile->rotations, i);
                m_rotations[i] = new float*[jointNum];
                for (Py_ssize_t j = 0; j < PyList_Size(frameRot); j++) {
                    PyObject* jointRot = PyList_GetItem(frameRot, j);
                    m_rotations[i][j] = new float[3];
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
        m_rootPositions = new float*[frameNum];
        if (PyList_Check(pyFile->rootPositions)) {
            for (Py_ssize_t i = 0; i < PyList_Size(pyFile->rootPositions); i++) {
                PyObject* jointRoot = PyList_GetItem(pyFile->rootPositions, i);
                m_rootPositions[i] = new float[3];
                for (Py_ssize_t j = 0; j < 3; j++) {
                    PyObject* valRoot = PyList_GetItem(jointRoot, j);
                    m_rootPositions[i][j] = (float)PyFloat_AsDouble(valRoot);
                }
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }
    }

    BVH_writer::BVH_writer(std::string staticDataPath){
        m_staticDataPath = staticDataPath;
    }

    void BVH_writer::write(float*** rotations, float** positions, float frametime, int frameNum, std::string writePath){
        if (PyImport_AppendInittab("cython_interface", PyInit_cython_interface) != 0) {
            fprintf(stderr, "Unable to extend Python inittab");
        }
        Py_Initialize();   // initialize Python
        std::string file_path = __FILE__;
        std::string dir_path = file_path.substr(0, file_path.rfind("\\"));
        std::replace(dir_path.begin(), dir_path.end(), '\\', '/');
        std::string src_path = "'" + dir_path + std::string("/bvh_python/pySrc") + "'";
        std::string pyLine = std::string("sys.path.append(") + src_path + std::string(")");
        PyRun_SimpleString("import sys");
        PyRun_SimpleString(pyLine.data());

        if (PyImport_ImportModule("cython_interface") == NULL) {
            fprintf(stderr, "Unable to import cython module.\n");
            if (PyErr_Occurred()) {
                PyErr_PrintEx(0);
            }
            else {
                fprintf(stderr, "Unknown error");
            }
        }


        BVH_writer_interface* pyWriterPtr = createWriterInterface(PyUnicode_FromString(m_staticDataPath.data()));
        int jointNum = pyWriterPtr->jointNum;
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

        writeBVH_interface(pyWriterPtr, frameListRot, frameListPos, PyUnicode_FromString(writePath.data()), PyFloat_FromDouble((double)frametime));
        Py_FinalizeEx();
    }

}

