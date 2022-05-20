#include "BVHManager.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include "../Core/Log.hpp"

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
        //Al cerrar el motor se llama esta funci�n donde se limpia el mapa de animaciones
        m_bvhDataMap.clear();
        Py_FinalizeEx();
    }

    void BVHManager::CleanUnusedBVHClips() noexcept {
        /*
        * Elimina todos los punteros del mapa cuyo conteo de referencias es igual a uno,
        * es decir, que el puntero del mapa es el unico que apunta a esa memoria.
        */
        for (auto i = m_bvhDataMap.begin(), last = m_bvhDataMap.end(); i != last;) {
            if (i->second.use_count() == 1) {
                i = m_bvhDataMap.erase(i);
            }
            else {
                ++i;
            }

        }
    }
    std::shared_ptr<BVHData> BVHManager::readBVH(std::shared_ptr<AnimationClip> animation) {
        BVHData* dataPtr = new BVHData(animation);
        std::shared_ptr<BVHData> sharedPtr = std::shared_ptr<BVHData>(dataPtr);
        m_bvhDataMap.insert({ {animation->GetSkeleton()->GetModelName(), animation->GetAnimationName()}, sharedPtr });
        return sharedPtr;
    }
    std::shared_ptr<BVHData> BVHManager::readBVH(std::string modelName, std::string animName) {
        BVHData* dataPtr = new BVHData(modelName, animName);
        std::shared_ptr<BVHData> sharedPtr = std::shared_ptr<BVHData>(dataPtr);
        m_bvhDataMap.insert({ {modelName, animName}, sharedPtr });
        return sharedPtr;
    }
    void BVHManager::writeBVHDynamicData(std::shared_ptr<BVHData> data, std::string outAnimName) {
        std::string writePath = BVHData::_getFilePath(data->getModelName(), outAnimName);
        
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
    BVHData::BVHData(std::string modelName, std::string animName) {
        std::string filePath = _getFilePath(modelName, animName);
        if (!std::filesystem::exists(filePath)) { 
            MONA_LOG_ERROR("BVHFile Error: Path does not exist -> {0}", filePath);
            return;
        }
        m_modelName = modelName;
        m_animName = animName;
        BVH_file_interface* pyFilePtr = createFileInterface(PyUnicode_FromString(filePath.data()), PyBool_FromLong(0), PyBool_FromLong(1));
        initFile(pyFilePtr);
        setDynamicData(m_rotations, m_rootPositions, m_frametime);
    }
    BVHData::BVHData(std::shared_ptr<AnimationClip> animation): BVHData(animation->GetSkeleton()->GetModelName(), animation->GetAnimationName()) {}

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
            MONA_LOG_ERROR("Passed PyObject pointer was not a list!");
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
            MONA_LOG_ERROR("Passed PyObject pointer was not a list!");
        }
        
        

        // offsets
        m_offsets = MatrixXf(jointNum, 3);
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
            MONA_LOG_ERROR("Passed PyObject pointer was not a list!");
        }

        // rotations
        m_rotations = std::vector<MatrixXf>(frameNum);//new float**[frameNum];
        if (PyList_Check(pyFile->rotations)) {
            for (Py_ssize_t i = 0; i <frameNum; i++) {
                PyObject* frameRot = PyList_GetItem(pyFile->rotations, i);
                m_rotations[i] = MatrixXf(jointNum, 4);
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
            MONA_LOG_ERROR("Passed PyObject pointer was not a list!");
        }


        // positions
        m_rootPositions = std::vector<VectorXf>(frameNum);
        if (PyList_Check(pyFile->rootPositions)) {
            for (Py_ssize_t i = 0; i < frameNum; i++) {
                PyObject* frameRoot = PyList_GetItem(pyFile->rootPositions, i);
                m_rootPositions[i] = VectorXf(3);
                for (Py_ssize_t j = 0; j < 3; j++) {
                    PyObject* valRoot = PyList_GetItem(frameRoot, j);
                    m_rootPositions[i](j) = (float)PyFloat_AsDouble(valRoot);
                }

            }
        }
        else {
            MONA_LOG_ERROR("Passed PyObject pointer was not a list!");
        }
    }

    void BVHData::setDynamicData(std::vector<MatrixXf> rotations, std::vector<VectorXf> rootPositions, float frametime) {
        if (rotations.size() != rootPositions.size() || rotations.size() != m_frameNum) {
            MONA_LOG_ERROR("Dynamic data must fit static data!");
        }
        for (int i = 0; i < m_frameNum; i++) {
            if (rotations[i].rows() != m_jointNum || rotations[i].cols() != 4) {
                MONA_LOG_ERROR("Dynamic data must fit static data!");
            }
            if (rootPositions[i].size() != 3) {
                MONA_LOG_ERROR("Dynamic data must fit static data!");
            }
        }
        m_rootPositions_dmic = rootPositions;
        m_rotations_dmic = rotations;
        m_frametime_dmic = frametime;
    }

    std::vector<JointPose> BVHData::getFramePoses(int frame) {
        std::vector<JointPose> poses(m_jointNum);
        for (int i = 0; i < m_jointNum; i++) {
            glm::vec3 tr(m_offsets(i,0), m_offsets(i,1), m_offsets(i,2));
            glm::vec3 scl(1, 1, 1);
            glm::fquat rot(m_rotations_dmic[frame](i, 0), m_rotations_dmic[frame](i, 1), m_rotations_dmic[frame](i, 2), m_rotations_dmic[frame](i, 3));
            poses[i] = JointPose(rot, tr, scl);
        }
        return poses;
    }

}

