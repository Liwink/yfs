project(lab)
cmake_minimum_required(VERSION 3.12)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/rpc)
include_directories("/usr/local/include/fuse")

add_library(yfs STATIC rpc/rpc.h rpc/rpc.cc lock_client.cc lock_client.h lock_server.cc lock_server.h 
        lock_smain.cc lock_tester.cc fuse.cc yfs_client.cc yfs_client.h extent_client.cc extent_client.h
        extent_server.h extent_server.cc extent_smain.cc
        lock_client_cache.cc lock_client_cache.h lock_server_cache.cc lock_server_cache.h
        handle.cc handle.h
        paxos_protocol.h paxos.cc paxos.h log.cc log.h config.cc config.h rsm.cc rsm.h rsm_protocol.h
        )
# add_library(sqlite3 STATIC ext/rtree/rtree.c ext/rtree/rtree.h src/vtab.c src/loadext.c)
# add_library(sqlite3 STATIC ext/rtree/rtree.c ext/rtree/rtree.h src/vtab.c src/vdbe.c src/vdbeapi.c src/where.c
#         src/select.c src/vdbeblob.c)

