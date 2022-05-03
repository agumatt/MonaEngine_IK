#include "BVHManager.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>

namespace Mona{

    // BVHManager

    void BVHManager::StartUp() {
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
    }
    void BVHManager::ShutDown() {
        for (int i = 0; i < GetInstance().m_readDataVector.size(); i++) {
            delete GetInstance().m_readDataVector[i];
        }
        Py_FinalizeEx();
    }

    BVHData* BVHManager::readBVH(std::string filePath) {
        BVHData* data = new BVHData(filePath);
        GetInstance().m_readDataVector.push_back(data);
        return data;
    }
    BVHData* BVHManager::readBVH(std::string filePath, std::vector<std::string> jointNames) {
        BVHData* data = new BVHData(filePath);
        GetInstance().m_readDataVector.push_back(data);
        return data;
    }
    void  BVHManager::writeBVHDynamicData(BVHData* data, std::string writePath) {
        BVH_writer_interface* pyWriterPtr = createWriterInterface(PyUnicode_FromString(data->getInputFilePath().data()));
        // rotations
        PyObject* frameListRot = PyList_New(data->getFrameNum());
        for (unsigned int i = 0; i < data->getFrameNum(); i++) {
            PyObject* jointListRot = PyList_New(data->getJointNum());
            PyList_SET_ITEM(frameListRot, i, jointListRot);
            for (unsigned int j = 0; j < data->getJointNum(); j++) {
                PyObject* valListRot = PyList_New(4);
                PyList_SET_ITEM(jointListRot, j, valListRot);
                for (unsigned int k = 0; k < 4; k++) {
                    PyObject* valRot = PyFloat_FromDouble((double)data->getDynamicRotations()[i](j,k));
                    PyList_SET_ITEM(valListRot, k, valRot);
                }
            }
        }


        // positions
        PyObject* frameListPos = PyList_New(data->getFrameNum());
        for (unsigned int i = 0; i < data->getFrameNum(); i++) {
            PyObject* rootPos = PyList_New(3);
            PyList_SET_ITEM(frameListPos, i, rootPos);
            for (unsigned int j = 0; j < 3; j++) {
                PyObject* valRootPos = PyFloat_FromDouble((double)data->getDynamicRootPositions()[i](j));
                PyList_SET_ITEM(rootPos, j, valRootPos);
            }
        }
        writeBVH_interface(pyWriterPtr, frameListRot, frameListPos, PyUnicode_FromString(writePath.data()), PyFloat_FromDouble((double)data->getDynamicFrametime()), PyBool_FromLong(1));
    }



    //BVHData

    BVHData::BVHData(std::string filePath, std::vector<std::string> jointNames){
        m_inputFilePath = filePath;
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
        BVH_file_interface* pyFilePtr = createFileInterface(PyUnicode_FromString(filePath.data()), listObj, PyBool_FromLong(1));
        initFile(pyFilePtr);
        setDynamicData(m_rotations, m_rootPositions, m_frametime);
    }

    BVHData::BVHData(std::string filePath) {
        m_inputFilePath = filePath;
        BVH_file_interface* pyFilePtr = createFileInterface(PyUnicode_FromString(filePath.data()), PyBool_FromLong(0), PyBool_FromLong(1));
        initFile(pyFilePtr);
        setDynamicData(m_rotations, m_rootPositions, m_frametime);
    }

    void BVHData::initFile(BVH_file_interface* pyFile) {
        m_jointNum = pyFile->jointNum;
        m_frameNum = pyFile->frameNum;
        m_frametime = pyFile->frametime;

        int jointNum = m_jointNum;
        int frameNum = m_frameNum;

        // topology
        m_topology = std::vector<int>(jointNum);
        if (PyList_Check(pyFile->topology)) {
            for (Py_ssize_t i = 0; i < jointNum; i++) {
                PyObject* value = PyList_GetItem(pyFile->topology, i);
                m_topology[i] = (int)PyLong_AsLong(value);
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }

        // jointNames
        m_jointNames = std::vector<std::string>(jointNum);
        if (PyList_Check(pyFile->jointNames)) {
            for (Py_ssize_t i = 0; i < jointNum; i++) {
                PyObject* name = PyList_GetItem(pyFile->jointNames, i);
                m_jointNames[i]= PyUnicode_AsUTF8(name);
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }
        
        

        // offsets
        m_offsets = MatrixXf(jointNum, 3);//new float*[jointNum];
        if (PyList_Check(pyFile->offsets)) {
            for (Py_ssize_t i = 0; i < jointNum; i++) {
                PyObject* jointOff = PyList_GetItem(pyFile->offsets, i);
                for (Py_ssize_t j = 0; j < 3; j++) {
                    PyObject* valOff = PyList_GetItem(jointOff, j);
                    m_offsets(i,j) = (float)PyFloat_AsDouble(valOff);
                }

            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }

        // rotations
        m_rotations = std::vector<MatrixXf>(frameNum);//new float**[frameNum];
        if (PyList_Check(pyFile->rotations)) {
            for (Py_ssize_t i = 0; i <frameNum; i++) {
                PyObject* frameRot = PyList_GetItem(pyFile->rotations, i);
                m_rotations[i] = MatrixXf(jointNum, 4);// = new float* [jointNum];
                for (Py_ssize_t j = 0; j < jointNum; j++) {
                    PyObject* jointRot = PyList_GetItem(frameRot, j);
                    for (Py_ssize_t k = 0; k < 4; k++) {
                        PyObject* valRot = PyList_GetItem(jointRot, k);
                        m_rotations[i](j,k) = (float)PyFloat_AsDouble(valRot);
                    }
                }
            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }


        // positions
        m_rootPositions = std::vector<VectorXf>(frameNum); //new float* [frameNum];
        if (PyList_Check(pyFile->rootPositions)) {
            for (Py_ssize_t i = 0; i < frameNum; i++) {
                PyObject* frameRoot = PyList_GetItem(pyFile->rootPositions, i);
                m_rootPositions[i] = VectorXf(3); // = new float[3];
                for (Py_ssize_t j = 0; j < 3; j++) {
                    PyObject* valRoot = PyList_GetItem(frameRoot, j);
                    m_rootPositions[i](j) = (float)PyFloat_AsDouble(valRoot);
                }

            }
        }
        else {
            throw new std::exception("Passed PyObject pointer was not a list!");
        }
    }

    void BVHData::setDynamicData(std::vector<MatrixXf> rotations, std::vector<VectorXf> rootPositions, float frametime) {
        if (rotations.size() != rootPositions.size() || rotations.size() != m_frameNum) {
            throw new std::exception("Dynamic data must fit static data!");
        }
        for (int i = 0; i < m_frameNum; i++) {
            if (rotations[i].size1() != m_jointNum || rotations[i].size2() != 4) {
                throw new std::exception("Dynamic data must fit static data!");
            }
            if (rootPositions[i].size() != 3) {
                throw new std::exception("Dynamic data must fit static data!");
            }
        }
        m_rootPositions_dmic = rootPositions;
        m_rotations_dmic = rotations;
        m_frametime_dmic = frametime;
    }

}

