#!/usr/bin/env python
# -*- coding: utf-8 -*-

#
# Script to visualize python output files.
#

import sys
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D


fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')


chaste_tets = [[[0,0.01,0],[1,0.02,0],[1,1,1],[1,0,1],[0.5,0,0],[1,0.5,0.5],[0.5,0.5,0.5],[0.5,0,0.5],[1,0,0.5],[1,0.5,1],],
[[0,0.01,0],[1,0.02,0],[1,1.12,0],[1,1,1],[0.5,0,0],[1,0.5,0],[0.5,0.5,0],[0.5,0.5,0.5],[1,0.5,0.5],[1,1,0.5],],
[[0,0.01,0],[0,1.11,0],[1,1,1],[1,1.12,0],[0,0.5,0],[0.5,1,0.5],[0.5,0.5,0.5],[0.5,0.5,0],[0.5,1,0],[1,1,0.5],],
[[0,0.01,0],[0,1.11,0],[0,1,1],[1,1,1],[0,0.5,0],[0,1,0.5],[0,0.5,0.5],[0.5,0.5,0.5],[0.5,1,0.5],[0.5,1,1],],
[[0,0.01,0],[0,0,1],[1,1,1],[0,1,1],[0,0,0.5],[0.5,0.5,1],[0.5,0.5,0.5],[0,0.5,0.5],[0,0.5,1],[0.5,1,1],],
[[0,0.01,0],[0,0,1],[1,0,1],[1,1,1],[0,0,0.5],[0.5,0,1],[0.5,0,0.5],[0.5,0.5,0.5],[0.5,0.5,1],[1,0.5,1],],
[[1,0.02,0],[2,0.03,0],[2,1,1],[2,0,1],[1.5,0,0],[2,0.5,0.5],[1.5,0.5,0.5],[1.5,0,0.5],[2,0,0.5],[2,0.5,1],],
[[1,0.02,0],[2,0.03,0],[2,1.13,0],[2,1,1],[1.5,0,0],[2,0.5,0],[1.5,0.5,0],[1.5,0.5,0.5],[2,0.5,0.5],[2,1,0.5],],
[[1,0.02,0],[1,1.12,0],[2,1,1],[2,1.13,0],[1,0.5,0],[1.5,1,0.5],[1.5,0.5,0.5],[1.5,0.5,0],[1.5,1,0],[2,1,0.5],],
[[1,0.02,0],[1,1.12,0],[1,1,1],[2,1,1],[1,0.5,0],[1,1,0.5],[1,0.5,0.5],[1.5,0.5,0.5],[1.5,1,0.5],[1.5,1,1],],
[[1,0.02,0],[1,0,1],[2,1,1],[1,1,1],[1,0,0.5],[1.5,0.5,1],[1.5,0.5,0.5],[1,0.5,0.5],[1,0.5,1],[1.5,1,1],],
[[1,0.02,0],[1,0,1],[2,0,1],[2,1,1],[1,0,0.5],[1.5,0,1],[1.5,0,0.5],[1.5,0.5,0.5],[1.5,0.5,1],[2,0.5,1],],
[[0,1.11,0],[1,1.12,0],[1,2,1],[1,1,1],[0.5,1,0],[1,1.5,0.5],[0.5,1.5,0.5],[0.5,1,0.5],[1,1,0.5],[1,1.5,1],],
[[0,1.11,0],[1,1.12,0],[1,2,0],[1,2,1],[0.5,1,0],[1,1.5,0],[0.5,1.5,0],[0.5,1.5,0.5],[1,1.5,0.5],[1,2,0.5],],
[[0,1.11,0],[0,2,0],[1,2,1],[1,2,0],[0,1.5,0],[0.5,2,0.5],[0.5,1.5,0.5],[0.5,1.5,0],[0.5,2,0],[1,2,0.5],],
[[0,1.11,0],[0,2,0],[0,2,1],[1,2,1],[0,1.5,0],[0,2,0.5],[0,1.5,0.5],[0.5,1.5,0.5],[0.5,2,0.5],[0.5,2,1],],
[[0,1.11,0],[0,1,1],[1,2,1],[0,2,1],[0,1,0.5],[0.5,1.5,1],[0.5,1.5,0.5],[0,1.5,0.5],[0,1.5,1],[0.5,2,1],],
[[0,1.11,0],[0,1,1],[1,1,1],[1,2,1],[0,1,0.5],[0.5,1,1],[0.5,1,0.5],[0.5,1.5,0.5],[0.5,1.5,1],[1,1.5,1],],
[[1,1.12,0],[2,1.13,0],[2,2,1],[2,1,1],[1.5,1,0],[2,1.5,0.5],[1.5,1.5,0.5],[1.5,1,0.5],[2,1,0.5],[2,1.5,1],],
[[1,1.12,0],[2,1.13,0],[2,2,0],[2,2,1],[1.5,1,0],[2,1.5,0],[1.5,1.5,0],[1.5,1.5,0.5],[2,1.5,0.5],[2,2,0.5],],
[[1,1.12,0],[1,2,0],[2,2,1],[2,2,0],[1,1.5,0],[1.5,2,0.5],[1.5,1.5,0.5],[1.5,1.5,0],[1.5,2,0],[2,2,0.5],],
[[1,1.12,0],[1,2,0],[1,2,1],[2,2,1],[1,1.5,0],[1,2,0.5],[1,1.5,0.5],[1.5,1.5,0.5],[1.5,2,0.5],[1.5,2,1],],
[[1,1.12,0],[1,1,1],[2,2,1],[1,2,1],[1,1,0.5],[1.5,1.5,1],[1.5,1.5,0.5],[1,1.5,0.5],[1,1.5,1],[1.5,2,1],],
[[1,1.12,0],[1,1,1],[2,1,1],[2,2,1],[1,1,0.5],[1.5,1,1],[1.5,1,0.5],[1.5,1.5,0.5],[1.5,1.5,1],[2,1.5,1],],
[[0,0,1],[1,0,1],[1,1,2],[1,0,2],[0.5,0,1],[1,0.5,1.5],[0.5,0.5,1.5],[0.5,0,1.5],[1,0,1.5],[1,0.5,2],],
[[0,0,1],[1,0,1],[1,1,1],[1,1,2],[0.5,0,1],[1,0.5,1],[0.5,0.5,1],[0.5,0.5,1.5],[1,0.5,1.5],[1,1,1.5],],
[[0,0,1],[0,1,1],[1,1,2],[1,1,1],[0,0.5,1],[0.5,1,1.5],[0.5,0.5,1.5],[0.5,0.5,1],[0.5,1,1],[1,1,1.5],],
[[0,0,1],[0,1,1],[0,1,2],[1,1,2],[0,0.5,1],[0,1,1.5],[0,0.5,1.5],[0.5,0.5,1.5],[0.5,1,1.5],[0.5,1,2],],
[[0,0,1],[0,0,2],[1,1,2],[0,1,2],[0,0,1.5],[0.5,0.5,2],[0.5,0.5,1.5],[0,0.5,1.5],[0,0.5,2],[0.5,1,2],],
[[0,0,1],[0,0,2],[1,0,2],[1,1,2],[0,0,1.5],[0.5,0,2],[0.5,0,1.5],[0.5,0.5,1.5],[0.5,0.5,2],[1,0.5,2],],
[[1,0,1],[2,0,1],[2,1,2],[2,0,2],[1.5,0,1],[2,0.5,1.5],[1.5,0.5,1.5],[1.5,0,1.5],[2,0,1.5],[2,0.5,2],],
[[1,0,1],[2,0,1],[2,1,1],[2,1,2],[1.5,0,1],[2,0.5,1],[1.5,0.5,1],[1.5,0.5,1.5],[2,0.5,1.5],[2,1,1.5],],
[[1,0,1],[1,1,1],[2,1,2],[2,1,1],[1,0.5,1],[1.5,1,1.5],[1.5,0.5,1.5],[1.5,0.5,1],[1.5,1,1],[2,1,1.5],],
[[1,0,1],[1,1,1],[1,1,2],[2,1,2],[1,0.5,1],[1,1,1.5],[1,0.5,1.5],[1.5,0.5,1.5],[1.5,1,1.5],[1.5,1,2],],
[[1,0,1],[1,0,2],[2,1,2],[1,1,2],[1,0,1.5],[1.5,0.5,2],[1.5,0.5,1.5],[1,0.5,1.5],[1,0.5,2],[1.5,1,2],],
[[1,0,1],[1,0,2],[2,0,2],[2,1,2],[1,0,1.5],[1.5,0,2],[1.5,0,1.5],[1.5,0.5,1.5],[1.5,0.5,2],[2,0.5,2],],
[[0,1,1],[1,1,1],[1,2,2],[1,1,2],[0.5,1,1],[1,1.5,1.5],[0.5,1.5,1.5],[0.5,1,1.5],[1,1,1.5],[1,1.5,2],],
[[0,1,1],[1,1,1],[1,2,1],[1,2,2],[0.5,1,1],[1,1.5,1],[0.5,1.5,1],[0.5,1.5,1.5],[1,1.5,1.5],[1,2,1.5],],
[[0,1,1],[0,2,1],[1,2,2],[1,2,1],[0,1.5,1],[0.5,2,1.5],[0.5,1.5,1.5],[0.5,1.5,1],[0.5,2,1],[1,2,1.5],],
[[0,1,1],[0,2,1],[0,2,2],[1,2,2],[0,1.5,1],[0,2,1.5],[0,1.5,1.5],[0.5,1.5,1.5],[0.5,2,1.5],[0.5,2,2],],
[[0,1,1],[0,1,2],[1,2,2],[0,2,2],[0,1,1.5],[0.5,1.5,2],[0.5,1.5,1.5],[0,1.5,1.5],[0,1.5,2],[0.5,2,2],],
[[0,1,1],[0,1,2],[1,1,2],[1,2,2],[0,1,1.5],[0.5,1,2],[0.5,1,1.5],[0.5,1.5,1.5],[0.5,1.5,2],[1,1.5,2],],
[[1,1,1],[2,1,1],[2,2,2],[2,1,2],[1.5,1,1],[2,1.5,1.5],[1.5,1.5,1.5],[1.5,1,1.5],[2,1,1.5],[2,1.5,2],],
[[1,1,1],[2,1,1],[2,2,1],[2,2,2],[1.5,1,1],[2,1.5,1],[1.5,1.5,1],[1.5,1.5,1.5],[2,1.5,1.5],[2,2,1.5],],
[[1,1,1],[1,2,1],[2,2,2],[2,2,1],[1,1.5,1],[1.5,2,1.5],[1.5,1.5,1.5],[1.5,1.5,1],[1.5,2,1],[2,2,1.5],],
[[1,1,1],[1,2,1],[1,2,2],[2,2,2],[1,1.5,1],[1,2,1.5],[1,1.5,1.5],[1.5,1.5,1.5],[1.5,2,1.5],[1.5,2,2],],
[[1,1,1],[1,1,2],[2,2,2],[1,2,2],[1,1,1.5],[1.5,1.5,2],[1.5,1.5,1.5],[1,1.5,1.5],[1,1.5,2],[1.5,2,2],],
[[1,1,1],[1,1,2],[2,1,2],[2,2,2],[1,1,1.5],[1.5,1,2],[1.5,1,1.5],[1.5,1.5,1.5],[1.5,1.5,2],[2,1.5,2],],
]

chaste_nodes = [[0,0.01,0],[1,0.02,0],[2,0.03,0],[0,1.11,0],[1,1.12,0],[2,1.13,0],[0,2,0],[1,2,0],[2,2,0],[0,0,1],[1,0,1],[2,0,1],[0,1,1],[1,1,1],[2,1,1],[0,2,1],[1,2,1],[2,2,1],[0,0,2],[1,0,2],[2,0,2],[0,1,2],[1,1,2],[2,1,2],[0,2,2],[1,2,2],[2,2,2],[0.5,0,0],[1.5,0,0],[0,0.5,0],[0.5,0.5,0],[1,0.5,0],[1.5,0.5,0],[2,0.5,0],[0.5,1,0],[1.5,1,0],[0,1.5,0],[0.5,1.5,0],[1,1.5,0],[1.5,1.5,0],[2,1.5,0],[0.5,2,0],[1.5,2,0],[0,0,0.5],[0.5,0,0.5],[1,0,0.5],[1.5,0,0.5],[2,0,0.5],[0,0.5,0.5],[0.5,0.5,0.5],[1,0.5,0.5],[1.5,0.5,0.5],[2,0.5,0.5],[0,1,0.5],[0.5,1,0.5],[1,1,0.5],[1.5,1,0.5],[2,1,0.5],[0,1.5,0.5],[0.5,1.5,0.5],[1,1.5,0.5],[1.5,1.5,0.5],[2,1.5,0.5],[0,2,0.5],[0.5,2,0.5],[1,2,0.5],[1.5,2,0.5],[2,2,0.5],[0.5,0,1],[1.5,0,1],[0,0.5,1],[0.5,0.5,1],[1,0.5,1],[1.5,0.5,1],[2,0.5,1],[0.5,1,1],[1.5,1,1],[0,1.5,1],[0.5,1.5,1],[1,1.5,1],[1.5,1.5,1],[2,1.5,1],[0.5,2,1],[1.5,2,1],[0,0,1.5],[0.5,0,1.5],[1,0,1.5],[1.5,0,1.5],[2,0,1.5],[0,0.5,1.5],[0.5,0.5,1.5],[1,0.5,1.5],[1.5,0.5,1.5],[2,0.5,1.5],[0,1,1.5],[0.5,1,1.5],[1,1,1.5],[1.5,1,1.5],[2,1,1.5],[0,1.5,1.5],[0.5,1.5,1.5],[1,1.5,1.5],[1.5,1.5,1.5],[2,1.5,1.5],[0,2,1.5],[0.5,2,1.5],[1,2,1.5],[1.5,2,1.5],[2,2,1.5],[0.5,0,2],[1.5,0,2],[0,0.5,2],[0.5,0.5,2],[1,0.5,2],[1.5,0.5,2],[2,0.5,2],[0.5,1,2],[1.5,1,2],[0,1.5,2],[0.5,1.5,2],[1,1.5,2],[1.5,1.5,2],[2,1.5,2],[0.5,2,2],[1.5,2,2],]


x_list = [x for [x,y,z] in chaste_nodes]
y_list = [y for [x,y,z] in chaste_nodes]
z_list = [z for [x,y,z] in chaste_nodes]

# points
ax.scatter(x_list, y_list, z_list, c='r',s=20)

for i,node in enumerate(chaste_nodes):
  ax.text(node[0], node[1], node[2], "{}".format(i), color='red')


for tet in chaste_tets:
  x_list = [x for [x,y,z] in tet]
  y_list = [y for [x,y,z] in tet]
  z_list = [z for [x,y,z] in tet]

  # points
  ax.scatter(x_list, y_list, z_list, c='m',s=10)

  # lines
  line_indices = [[0,1],[0,2],[0,3],[1,2],[1,3],[2,3]]
  x_lines_list = []
  y_lines_list = []
  z_lines_list = []
  for [i1,i2] in line_indices:
    x_lines_list += [x_list[i1], x_list[i2], np.inf]
    y_lines_list += [y_list[i1], y_list[i2], np.inf]
    z_lines_list += [z_list[i1], z_list[i2], np.inf]
  ax.plot(x_lines_list, y_lines_list, z_lines_list, c='y')

  

ax.set_xlabel('x')
ax.set_ylabel('y')
ax.set_zlabel('z')



plt.show();
