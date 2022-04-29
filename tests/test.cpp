#include "./CharacterNavigation/BVHManager.hpp"
#include <iostream>
#include <algorithm>


    
int main(){
    std::string file_path = __FILE__;
    std::string dir_path = file_path.substr(0, file_path.rfind("\\"));
    std::replace(dir_path.begin(), dir_path.end(), '\\', '/');
    Mona::BVH_file file = Mona::BVH_file::BVH_file(dir_path + std::string("/BigVegas.bvh"));
    Mona::BVH_writer writer = Mona::BVH_writer::BVH_writer(dir_path + std::string("/BigVegas.bvh"));
    writer.write(file.m_rotations, file.m_rootPositions, file.m_frametime, file.m_frameNum, dir_path + std::string("/BigVegasCopy.bvh"));
    return 0;
}

