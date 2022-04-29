import numpy as np
import BVH
from Quaternions import Quaternions

class BVH_file:
    def __init__(self, filePath, jointNames = None):
        self.anim, self._names, self.frametime = BVH.load(filePath)
        self.edges = [] #incluye offsets
        self.edge_mat = []
        self.edge_num = 0
        self._topology = None
        self.ee_length = []
        if jointNames:
            self.subsetNames = jointNames
        else:
            self.subsetNames = self._names

        for i, name in enumerate(self._names):
            if ':' in name:
                name = name[name.find(':') + 1:]
                self._names[i] = name


        self.details = [i for i, name in enumerate(self._names) if name not in self.subsetNames]
        self.joint_num = self.anim.shape[1]
        self.joints = []
        self.simplified_name = []
        self.simplify_map = {}
        self.inverse_simplify_map = {}

        for name in self.subsetNames:
            for j in range(self.anim.shape[1]):
                if name == self._names[j]:
                    self.joints.append(j)
                    break

        if len(self.joints) != len(self.subsetNames):
            for i in self.joints: print(self._names[i], end=' ')
            print(self.joints, len(self.joints), sep='\n')
            raise Exception('Problem in file', filePath)

        self.joint_num_simplify = len(self.joints)
        for i, j in enumerate(self.joints):
            self.simplify_map[j] = i
            self.inverse_simplify_map[i] = j
            self.simplified_name.append(self._names[j])
        self.inverse_simplify_map[0] = -1
        for i in range(self.anim.shape[1]):
            if i in self.details:
                self.simplify_map[i] = -1


    def scale(self, alpha):
        self.anim.offsets *= alpha
        global_position = self.anim.positions[:, 0, :]
        global_position[1:, :] *= alpha
        global_position[1:, :] += (1 - alpha) * global_position[0, :]

    def rotate(self, theta, axis):
        q = Quaternions(np.hstack((np.cos(theta/2), np.sin(theta/2) * axis)))
        position = self.anim.positions[:, 0, :].copy()
        rotation = self.anim.rotations[:, 0, :]
        position[1:, ...] -= position[0:-1, ...]
        q_position = Quaternions(np.hstack((np.zeros((position.shape[0], 1)), position)))
        q_rotation = Quaternions.from_euler(np.radians(rotation))
        q_rotation = q * q_rotation
        q_position = q * q_position * (-q)
        self.anim.rotations[:, 0, :] = np.degrees(q_rotation.euler())
        position = q_position.imaginaries
        for i in range(1, position.shape[0]):
            position[i] += position[i-1]
        self.anim.positions[:, 0, :] = position

    @property
    def topology(self):
        if self._topology is None:
            # se extraen los padres de las joints filtradas
            self._topology = self.anim.parents[self.joints].copy()
            for i in range(self._topology.shape[0]):
                # con esto el id del padre referencia la posicion de la joint hija en self.corps
                # la raiz mantiene su padre con valor -1
                if i >= 1: self._topology[i] = self.simplify_map[self._topology[i]]
            self._topology = tuple(self._topology)
        return self._topology

    def get_positions(self):
        positions = self.anim.positions
        positions = positions[:, self.joints, :]
        return positions
    
    def get_root_positions(self):
        positions = self.anim.positions
        positions = positions[:, 0, :]
        return np.squeeze(positions)

    @property
    def offsets(self): #offsets incluye la raiz
        return self.anim.offsets[self.joints]

    @property
    def names(self):
        return self.simplified_name

    def write(self, file_path):
        motion = self.to_numpy(quater=False, edge=False)
        rotations = motion[..., :-3].reshape(motion.shape[0], -1, 3)
        positions = motion[..., -3:]
        BVH_writer.write_bvh(self.topology, self.offsets, rotations, positions, self.names, 1.0/30, 'xyz', file_path)


class BVH_writer:
    def __init__(self, stdbvhPath):
        print("writer1")
        staticFile = BVH_file(stdbvhPath)
        print("writer2")
        self.parent = staticFile.topology
        print("writer3")
        self.offset = staticFile.offsets
        print("writer4")
        self.names = staticFile.names
        print("writer5")
        self.joint_num = len(self.parent)
        print("writer6")

    # position, rotation with shape T * J * 3
    def write(self, rotations, positions, path, frametime=1.0/30):
        order = 'xyz'
        
        if rotations.shape[1] < self.joint_num:
            rootRot = np.zeros((rotations.shape[0], 1, 3))
            rotations = np.concatenate((rootRot, rotations), 1)

        return write_bvh(self.parent, self.offset, rotations, positions, self.names, frametime, order, path)

    def write_raw(self, motion, order, path, frametime=1.0/30):
        positions = motion[:, -3:]
        rotations = motion[:, :-3]
        if order == 'quaternion':
            rotations = rotations.reshape((motion.shape[0], -1, 4))
        else:
            rotations = rotations.reshape((motion.shape[0], -1, 3))

        return self.write(rotations, positions, order, path, frametime)
    
# rotation with shape frame * J * 3
def write_bvh(parent, offset, rotation, position, names, frametime, order, path, endsite=None): #offset, rotations,.. incluye la raiz 
    file = open(path, 'w')
    frame = rotation.shape[0]
    joint_num = rotation.shape[1]
    order = order.upper()

    file_string = 'HIERARCHY\n'

    def write_static(idx, prefix):
        nonlocal parent, offset, rotation, names, order, endsite, file_string
        if idx == 0:
            name_label = 'ROOT ' + names[idx]
            channel_label = 'CHANNELS 6 Xposition Yposition Zposition {}rotation {}rotation {}rotation'.format(*order)
        else:
            name_label = 'JOINT ' + names[idx]
            channel_label = 'CHANNELS 3 {}rotation {}rotation {}rotation'.format(*order)
        offset_label = 'OFFSET %.6f %.6f %.6f' % (offset[idx][0], offset[idx][1], offset[idx][2])

        file_string += prefix + name_label + '\n'
        file_string += prefix + '{\n'
        file_string += prefix + '\t' + offset_label + '\n'
        file_string += prefix + '\t' + channel_label + '\n'

        has_child = False
        for y in range(idx+1, rotation.shape[1]):
            if parent[y] == idx:
                has_child = True
                write_static(y, prefix + '\t')
        if not has_child:
            file_string += prefix + '\t' + 'End Site\n'
            file_string += prefix + '\t' + '{\n'
            file_string += prefix + '\t\t' + 'OFFSET 0 0 0\n'
            file_string += prefix + '\t' + '}\n'

        file_string += prefix + '}\n'

    write_static(0, '')

    file_string += 'MOTION\n' + 'Frames: {}\n'.format(frame) + 'Frame Time: %.8f\n' % frametime
    for i in range(frame):
        file_string += '%.6f %.6f %.6f ' % (position[i][0], position[i][1], position[i][2])
        for j in range(joint_num):
            file_string += '%.6f %.6f %.6f ' % (rotation[i][j][0], rotation[i][j][1], rotation[i][j][2])
        file_string += '\n'

    file.write(file_string)
    return file_string
