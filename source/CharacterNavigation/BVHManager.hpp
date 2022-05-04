#pragma once
#ifndef BVHMANAGER_HPP
#define BVHMANAGER_HPP
#define VIENNACL_WITH_OPENCL 1
#include "bvh_python/cython_interface.h"
#include <string>
#include <vector>
#include <memory>
#include "viennacl/matrix.hpp"
#include "viennacl/vector.hpp"
#include "viennacl/linalg/norm_2.hpp"

namespace Mona {
    typedef viennacl::matrix<float> MatrixXf;
    typedef viennacl::vector<float> VectorXf;

    struct BVHDynamicData {
        std::vector<VectorXf> rootPositions;
        std::vector<MatrixXf> rotations;
        int jointNum;
        int frameNum;
        float frametime;
    };

    class BVHData {
        private:
            friend class BVHManager;
            friend class IKRig;
            BVHData(std::string filePath);
            BVHData(std::string filePath, std::vector<std::string> jointNames);
            void initFile(BVH_file_interface* pyFile);
            MatrixXf m_offsets;
            std::vector<VectorXf> m_rootPositions;
            std::vector<MatrixXf> m_rotations;
            int m_jointNum;
            int m_frameNum;
            float m_frametime;
            std::string m_inputFilePath;
            std::vector<int> m_topology;
            std::vector<std::string> m_jointNames; // si se quiere usar un subset de las joints originales
            std::vector<VectorXf> m_rootPositions_dmic;
            std::vector<MatrixXf> m_rotations_dmic;
            float m_frametime_dmic;
        public:
            MatrixXf getOffsets() { return m_offsets; }
            std::vector<VectorXf> getRootPositions() { return m_rootPositions; }
            std::vector<MatrixXf> getRotations() { return m_rotations; }
            int getJointNum() { return m_jointNum; }
            int getFrameNum() { return m_frameNum; }
            float getFrametime() { return m_frametime; }
            std::vector<int> getTopology() { return m_topology;}
            std::vector<std::string> getJointNames() { return m_jointNames; }
            std::string getInputFilePath() { return m_inputFilePath;  }
            std::vector<VectorXf> getDynamicRootPositions() { return m_rootPositions_dmic; }
            std::vector<MatrixXf> getDynamicRotations() { return m_rotations_dmic; }
            float getDynamicFrametime() { return m_frametime_dmic; }
            void setDynamicData(std::vector<MatrixXf> rotations, std::vector<VectorXf> rootPositions, float frametime);
    };

    class BVHManager {
        private:
            BVHManager() = default;
            std::vector<BVHData*> m_readDataVector;
        public:
            BVHManager& operator=(BVHManager const&) = delete;
            static BVHData* readBVH(std::string filePath);
            static BVHData* readBVH(std::string filePath, std::vector<std::string> jointNames);
            void writeBVHDynamicData(BVHData* data, std::string writePath);
            static void StartUp();
            static void ShutDown();
            static BVHManager& GetInstance() noexcept {
                static BVHManager instance;
                return instance;
		    }


    };

}

#endif