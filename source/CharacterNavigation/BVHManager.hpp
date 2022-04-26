#pragma once
#ifndef BVHMANAGER_HPP
#define BVHMANAGER_HPP
#include <string>
#include <vector>
#include "cython_interface.h"

namespace Mona {

	class BVH_file {
        public:
            BVH_file(std::string &filePath);
            BVH_file(std::string& filePath, std::vector<std::string> jointNames);
            int* m_topology;
            std::string* m_jointNames; // si se quiere usar un subset de las joints originales
            float** m_offsets;
            float*** m_positions;
            float*** m_rotations;
            int m_jointNum;
            int m_frameNum;
            float m_frametime;
    private:
        void initFile(BVH_file_interface pyFile);

    };

    class BVH_writer {
        public:
            BVH_writer(std::string &staticDataPath);
            std::string m_staticDataPath;
            void write(float*** rotations, float** positions, float frametime, std::string& writePath);
    };


}

#endif