// Copyright (c) 2013, Sandia Corporation.
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
// 
//     * Neither the name of Sandia Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 


#include <stk_unit_tests/stk_mesh_fixtures/GridFixture.hpp>

#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/Bucket.hpp>
#include <stk_mesh/base/Selector.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/Types.hpp>

#include <stk_util/parallel/ParallelReduce.hpp>

/*
The grid fixture creates the mesh below and skins it
1-16 Quadrilateral<4>
17-41 Nodes
skin ids are generated by the distributed index

Note:  "=" and "||" represent side entities.

17===18===19===20===21
|| 1 |  2 |  3 |  4 ||
22---23---24---25---26
|| 5 |  6 |  7 |  8 ||
27---28---29---30---31
|| 9 | 10 | 11 | 12 ||
32---33---34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

This use case will iteratively erode the mesh.

Each iteration will move a selection of faces to the 'dead_part"
Create boundaries between live and dead faces
Destroy nodes and sides that are no longer attached to a live face

0:  Init the mesh

17===18===19===20===21
|| 1 |  2 |  3 |  4 ||
22---23---24---25---26
|| 5 |  6 |  7 |  8 ||
27---28---29---30---31
|| 9 | 10 | 11 | 12 ||
32---33---34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

1: Move 4, 9 and 10 to the dead part


17===18===19===20
|| 1 |  2 |  3 ||
22---23---24---25===26
|| 5 |  6 |  7 |  8 ||
27===28===29---30---31
          || 11| 12 ||
32===33===34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41


2: Move faces 2 and 3 to the dead part

17===18
|| 1 ||
22---23===24===25===26
|| 5 |  6 |  7 |  8 ||
27===28===29---30---31
          || 11| 12 ||
32===33===34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

3: Move faces 1 and 11 to the dead part

22===23===24===25===26
|| 5 |  6 |  7 |  8 ||
27===28===29===30---31
               || 12||
32===33===34===35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

4: Move faces 6 and 7 to the dead part

22===23        25===26
|| 5 ||        ||  8||
27===28        30---31
               || 12||
32===33===34===35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

5: Move faces 5 and 16 to the dead part

               25===26
               ||  8||
               30---31
               || 12||
32===33===34===35===36
|| 13| 14 | 15 ||
37===38===39===40

6: Move the remaining faces to the dead part


(this space intentionally left blank)
  (nothing to see here)

*/

const int NUM_ITERATIONS = 7;
const int NUM_RANK = 4;

typedef std::vector<stk::mesh::Entity> EntityVector;

// Validation constants:
const int global_num_dead[NUM_ITERATIONS][NUM_RANK] =
{ //nodes  edges  faces   elements
  {0,     0,     0,   0 }, //0
  {1,     3,     0,   3 }, //1
  {3,     6,     0,   5 }, //2
  {5,     10,    0,   7 }, //3
  {7,     14,    0,   9 }, //4
  {12,    20,    0,   11}, //5
  {25,    34,    0,   16}  //6
};

// Validation constants:
const int global_num_live[NUM_ITERATIONS][NUM_RANK] =
{ //nodes  edges  faces  elements
  {25,    16,    0,     16}, //0
  {24,    20,    0,     13}, //1
  {22,    20,    0,     11}, //2
  {20,    20,    0,     9 }, //3
  {18,    18,    0,     7 }, //4
  {13,    14,    0,     5 }, //5
  {0,     0,     0,     0 }  //6
};

//----------------------------------------------------------------------------------

//Generates a vector of entities to be killed in this iteration
EntityVector entities_to_be_killed(
    const stk::mesh::BulkData & mesh,
    int iteration,
    stk::mesh::EntityRank entity_rank
    ) {

  std::vector<unsigned> entity_ids_to_kill;
  switch(iteration) {
    case 0:
      break;
    case 1:
      entity_ids_to_kill.push_back(4);
      entity_ids_to_kill.push_back(9);
      entity_ids_to_kill.push_back(10);
      break;
    case 2:
      entity_ids_to_kill.push_back(2);
      entity_ids_to_kill.push_back(3);
      break;
    case 3:
      entity_ids_to_kill.push_back(1);
      entity_ids_to_kill.push_back(11);
      break;
    case 4:
      entity_ids_to_kill.push_back(6);
      entity_ids_to_kill.push_back(7);
      break;
    case 5:
      entity_ids_to_kill.push_back(5);
      entity_ids_to_kill.push_back(16);
      break;
    case 6:
      entity_ids_to_kill.push_back(8);
      entity_ids_to_kill.push_back(12);
      entity_ids_to_kill.push_back(13);
      entity_ids_to_kill.push_back(14);
      entity_ids_to_kill.push_back(15);
      break;
    default:
      break;
  }

  EntityVector entities_to_kill;
  for (std::vector<unsigned>::const_iterator itr = entity_ids_to_kill.begin();
      itr != entity_ids_to_kill.end(); ++itr) {
    stk::mesh::Entity temp = mesh.get_entity(entity_rank, *itr);
    //select the entity only if the current process in the owner
    if (mesh.is_valid(temp) && mesh.parallel_owner_rank(temp) == mesh.parallel_rank()) {
      entities_to_kill.push_back(temp);
    }
  }
  return entities_to_kill;
}


//----------------------------------------------------------------------------------
namespace {
  struct entity_side{

    unsigned entity_id;
    unsigned side_ordinal;

    entity_side( unsigned e, unsigned s)
      : entity_id(e), side_ordinal(s) {}
  };
}

bool validate_sides( stk::mesh::fixtures::GridFixture & fixture, int iteration)
{
  enum {
    LEFT   = 0,
    BOTTOM = 1,
    RIGHT  = 2,
    TOP    = 3
  };

  std::vector<entity_side> live_sides, dead_sides;
  switch(iteration)
  {
    case 0:
      {
        /*
           17===18===19===20===21
           || 1 |  2 |  3 |  4 ||
           22---23---24---25---26
           || 5 |  6 |  7 |  8 ||
           27---28---29---30---31
           || 9 | 10 | 11 | 12 ||
           32---33---34---35---36
           || 13| 14 | 15 | 16 ||
           37===38===39===40===41
        */
        live_sides.push_back(entity_side(1,TOP));
        live_sides.push_back(entity_side(2,TOP));
        live_sides.push_back(entity_side(3,TOP));
        live_sides.push_back(entity_side(4,TOP));
        live_sides.push_back(entity_side(4,RIGHT));
        live_sides.push_back(entity_side(8,RIGHT));
        live_sides.push_back(entity_side(12,RIGHT));
        live_sides.push_back(entity_side(16,RIGHT));
        live_sides.push_back(entity_side(16,BOTTOM));
        live_sides.push_back(entity_side(15,BOTTOM));
        live_sides.push_back(entity_side(14,BOTTOM));
        live_sides.push_back(entity_side(13,BOTTOM));
        live_sides.push_back(entity_side(13,LEFT));
        live_sides.push_back(entity_side(9,LEFT));
        live_sides.push_back(entity_side(5,LEFT));
        live_sides.push_back(entity_side(1,LEFT));
      }
      break;

    case 1:
      {
        /*
           17===18===19===20
           || 1 |  2 |  3 ||
           22---23---24---25===26
           || 5 |  6 |  7 |  8 ||
           27===28===29---30---31
                     || 11| 12 ||
           32===33===34---35---36
           || 13| 14 | 15 | 16 ||
           37===38===39===40===41
         */
        live_sides.push_back(entity_side(1,TOP));
        live_sides.push_back(entity_side(2,TOP));
        live_sides.push_back(entity_side(3,TOP));
        live_sides.push_back(entity_side(3,RIGHT));
        live_sides.push_back(entity_side(8,TOP));
        live_sides.push_back(entity_side(8,RIGHT));
        live_sides.push_back(entity_side(12,RIGHT));
        live_sides.push_back(entity_side(16,RIGHT));
        live_sides.push_back(entity_side(16,BOTTOM));
        live_sides.push_back(entity_side(15,BOTTOM));
        live_sides.push_back(entity_side(14,BOTTOM));
        live_sides.push_back(entity_side(13,BOTTOM));
        live_sides.push_back(entity_side(13,LEFT));
        live_sides.push_back(entity_side(13,TOP));
        live_sides.push_back(entity_side(14,TOP));
        live_sides.push_back(entity_side(11,LEFT));
        live_sides.push_back(entity_side(6,BOTTOM));
        live_sides.push_back(entity_side(5,BOTTOM));
        live_sides.push_back(entity_side(5,LEFT));
        live_sides.push_back(entity_side(1,LEFT));

        dead_sides.push_back(entity_side(4,TOP));
        dead_sides.push_back(entity_side(4,RIGHT));
        dead_sides.push_back(entity_side(9,LEFT));
      }
      break;

    case 2:
      {
        /*
           17===18
           || 1 ||
           22---23===24===25===26
           || 5 |  6 |  7 |  8 ||
           27===28===29---30---31
                     || 11| 12 ||
           32===33===34---35---36
           || 13| 14 | 15 | 16 ||
           37===38===39===40===41
        */
        live_sides.push_back(entity_side(1,TOP));
        live_sides.push_back(entity_side(1,RIGHT));
        live_sides.push_back(entity_side(6,TOP));
        live_sides.push_back(entity_side(7,TOP));
        live_sides.push_back(entity_side(8,TOP));
        live_sides.push_back(entity_side(8,RIGHT));
        live_sides.push_back(entity_side(12,RIGHT));
        live_sides.push_back(entity_side(16,RIGHT));
        live_sides.push_back(entity_side(16,BOTTOM));
        live_sides.push_back(entity_side(15,BOTTOM));
        live_sides.push_back(entity_side(14,BOTTOM));
        live_sides.push_back(entity_side(13,BOTTOM));
        live_sides.push_back(entity_side(13,LEFT));
        live_sides.push_back(entity_side(13,TOP));
        live_sides.push_back(entity_side(14,TOP));
        live_sides.push_back(entity_side(11,LEFT));
        live_sides.push_back(entity_side(6,BOTTOM));
        live_sides.push_back(entity_side(5,BOTTOM));
        live_sides.push_back(entity_side(5,LEFT));
        live_sides.push_back(entity_side(1,LEFT));

        dead_sides.push_back(entity_side(2,TOP));
        dead_sides.push_back(entity_side(3,TOP));
        dead_sides.push_back(entity_side(3,RIGHT));
        dead_sides.push_back(entity_side(4,TOP));
        dead_sides.push_back(entity_side(4,RIGHT));
        dead_sides.push_back(entity_side(9,LEFT));
      }
      break;

    case 3:
      {
        /*
           22===23===24===25===26
           || 5 |  6 |  7 |  8 ||
           27===28===29===30---31
                          || 12||
           32===33===34===35---36
           || 13| 14 | 15 | 16 ||
           37===38===39===40===41
         */
        live_sides.push_back(entity_side(5,TOP));
        live_sides.push_back(entity_side(6,TOP));
        live_sides.push_back(entity_side(7,TOP));
        live_sides.push_back(entity_side(8,TOP));
        live_sides.push_back(entity_side(8,RIGHT));
        live_sides.push_back(entity_side(12,RIGHT));
        live_sides.push_back(entity_side(16,RIGHT));
        live_sides.push_back(entity_side(16,BOTTOM));
        live_sides.push_back(entity_side(15,BOTTOM));
        live_sides.push_back(entity_side(14,BOTTOM));
        live_sides.push_back(entity_side(13,BOTTOM));
        live_sides.push_back(entity_side(13,LEFT));
        live_sides.push_back(entity_side(13,TOP));
        live_sides.push_back(entity_side(14,TOP));
        live_sides.push_back(entity_side(15,TOP));
        live_sides.push_back(entity_side(12,LEFT));
        live_sides.push_back(entity_side(7,BOTTOM));
        live_sides.push_back(entity_side(6,BOTTOM));
        live_sides.push_back(entity_side(5,BOTTOM));
        live_sides.push_back(entity_side(5,LEFT));

        dead_sides.push_back(entity_side(1,LEFT));
        dead_sides.push_back(entity_side(1,TOP));
        dead_sides.push_back(entity_side(1,RIGHT));
        dead_sides.push_back(entity_side(2,TOP));
        dead_sides.push_back(entity_side(3,TOP));
        dead_sides.push_back(entity_side(3,RIGHT));
        dead_sides.push_back(entity_side(4,TOP));
        dead_sides.push_back(entity_side(4,RIGHT));
        dead_sides.push_back(entity_side(9,LEFT));
        dead_sides.push_back(entity_side(11,LEFT));

      }
      break;

    case 4:
      {
        /*
           22===23        25===26
           || 5 ||        ||  8||
           27===28        30---31
                          || 12||
           32===33===34===35---36
           || 13| 14 | 15 | 16 ||
           37===38===39===40===41
         */
        live_sides.push_back(entity_side(5,LEFT));
        live_sides.push_back(entity_side(5,BOTTOM));
        live_sides.push_back(entity_side(5,RIGHT));
        live_sides.push_back(entity_side(5,TOP));
        live_sides.push_back(entity_side(8,TOP));
        live_sides.push_back(entity_side(8,RIGHT));
        live_sides.push_back(entity_side(12,RIGHT));
        live_sides.push_back(entity_side(16,RIGHT));
        live_sides.push_back(entity_side(16,BOTTOM));
        live_sides.push_back(entity_side(15,BOTTOM));
        live_sides.push_back(entity_side(14,BOTTOM));
        live_sides.push_back(entity_side(13,BOTTOM));
        live_sides.push_back(entity_side(13,LEFT));
        live_sides.push_back(entity_side(13,TOP));
        live_sides.push_back(entity_side(14,TOP));
        live_sides.push_back(entity_side(15,TOP));
        live_sides.push_back(entity_side(12,LEFT));
        live_sides.push_back(entity_side(8,LEFT));

        dead_sides.push_back(entity_side(1,LEFT));
        dead_sides.push_back(entity_side(1,RIGHT));
        dead_sides.push_back(entity_side(1,TOP));
        dead_sides.push_back(entity_side(2,TOP));
        dead_sides.push_back(entity_side(3,RIGHT));
        dead_sides.push_back(entity_side(3,TOP));
        dead_sides.push_back(entity_side(4,RIGHT));
        dead_sides.push_back(entity_side(4,TOP));
        dead_sides.push_back(entity_side(6,BOTTOM));
        dead_sides.push_back(entity_side(6,TOP));
        dead_sides.push_back(entity_side(7,BOTTOM));
        dead_sides.push_back(entity_side(7,TOP));
        dead_sides.push_back(entity_side(9,LEFT));
        dead_sides.push_back(entity_side(11,LEFT));
      }
      break;

    case 5:
      {
        /*
                          25===26
                          ||  8||
                          30---31
                          || 12||
           32===33===34===35===36
           || 13| 14 | 15 ||
           37===38===39===40
         */
        live_sides.push_back(entity_side(8,TOP));
        live_sides.push_back(entity_side(8,RIGHT));
        live_sides.push_back(entity_side(12,RIGHT));
        live_sides.push_back(entity_side(12,BOTTOM));
        live_sides.push_back(entity_side(15,RIGHT));
        live_sides.push_back(entity_side(15,BOTTOM));
        live_sides.push_back(entity_side(14,BOTTOM));
        live_sides.push_back(entity_side(13,BOTTOM));
        live_sides.push_back(entity_side(13,LEFT));
        live_sides.push_back(entity_side(13,TOP));
        live_sides.push_back(entity_side(14,TOP));
        live_sides.push_back(entity_side(15,TOP));
        live_sides.push_back(entity_side(12,LEFT));
        live_sides.push_back(entity_side(8,LEFT));

        dead_sides.push_back(entity_side(1,LEFT));
        dead_sides.push_back(entity_side(1,RIGHT));
        dead_sides.push_back(entity_side(1,TOP));
        dead_sides.push_back(entity_side(2,TOP));
        dead_sides.push_back(entity_side(3,RIGHT));
        dead_sides.push_back(entity_side(3,TOP));
        dead_sides.push_back(entity_side(4,RIGHT));
        dead_sides.push_back(entity_side(4,TOP));
        dead_sides.push_back(entity_side(5,LEFT));
        dead_sides.push_back(entity_side(5,BOTTOM));
        dead_sides.push_back(entity_side(5,RIGHT));
        dead_sides.push_back(entity_side(5,TOP));
        dead_sides.push_back(entity_side(6,BOTTOM));
        dead_sides.push_back(entity_side(6,TOP));
        dead_sides.push_back(entity_side(7,BOTTOM));
        dead_sides.push_back(entity_side(7,TOP));
        dead_sides.push_back(entity_side(9,LEFT));
        dead_sides.push_back(entity_side(11,LEFT));
        dead_sides.push_back(entity_side(16,RIGHT));
        dead_sides.push_back(entity_side(16,BOTTOM));
      }
      break;

    case 6:
      {
        /* (All dead) */
        dead_sides.push_back(entity_side(1,LEFT));
        dead_sides.push_back(entity_side(1,RIGHT));
        dead_sides.push_back(entity_side(1,TOP));
        dead_sides.push_back(entity_side(2,TOP));
        dead_sides.push_back(entity_side(3,RIGHT));
        dead_sides.push_back(entity_side(3,TOP));
        dead_sides.push_back(entity_side(4,RIGHT));
        dead_sides.push_back(entity_side(4,TOP));
        dead_sides.push_back(entity_side(5,LEFT));
        dead_sides.push_back(entity_side(5,BOTTOM));
        dead_sides.push_back(entity_side(5,RIGHT));
        dead_sides.push_back(entity_side(5,TOP));
        dead_sides.push_back(entity_side(6,BOTTOM));
        dead_sides.push_back(entity_side(6,TOP));
        dead_sides.push_back(entity_side(7,BOTTOM));
        dead_sides.push_back(entity_side(7,TOP));
        dead_sides.push_back(entity_side(8,LEFT));
        dead_sides.push_back(entity_side(8,TOP));
        dead_sides.push_back(entity_side(8,RIGHT));
        dead_sides.push_back(entity_side(9,LEFT));
        dead_sides.push_back(entity_side(11,LEFT));
        dead_sides.push_back(entity_side(12,RIGHT));
        dead_sides.push_back(entity_side(12,BOTTOM));
        dead_sides.push_back(entity_side(12,LEFT));
        dead_sides.push_back(entity_side(13,BOTTOM));
        dead_sides.push_back(entity_side(13,LEFT));
        dead_sides.push_back(entity_side(13,TOP));
        dead_sides.push_back(entity_side(14,BOTTOM));
        dead_sides.push_back(entity_side(14,TOP));
        dead_sides.push_back(entity_side(15,RIGHT));
        dead_sides.push_back(entity_side(15,BOTTOM));
        dead_sides.push_back(entity_side(15,TOP));
        dead_sides.push_back(entity_side(16,RIGHT));
        dead_sides.push_back(entity_side(16,BOTTOM));
      }
      break;

    default:
      break;
  }

  stk::mesh::BulkData& mesh = fixture.bulk_data();
  stk::mesh::Part & dead_part = *fixture.dead_part();
  const stk::mesh::EntityRank element_rank = stk::topology::ELEMENT_RANK;
  const stk::mesh::EntityRank side_rank = mesh.mesh_meta_data().side_rank();

  // Select live or dead from owned, shared, and ghosted
  stk::mesh::Selector select_dead = dead_part ;
  stk::mesh::Selector select_live = !dead_part ;

  //check live sides
  for (std::vector<entity_side>::const_iterator itr = live_sides.begin();
      itr != live_sides.end(); ++itr) {
    stk::mesh::Entity entity = mesh.get_entity(element_rank, itr->entity_id);
    if (mesh.is_valid(entity)) {
      //make sure the side exist
      const int side_ordinal = itr->side_ordinal;

      //      for (; existing_sides.first != existing_sides.second &&
      //          existing_sides.first->relation_ordinal() != side_ordinal ;
      //          ++existing_sides.first);
      //
      //      //reached the end or side is not live
      //      if (existing_sides.first == existing_sides.second
      //          || !select_live(mesh.bucket(existing_sides.first->entity())) )
      //      {
      //        return false;
      //      }
      stk::mesh::Entity const* existing_sides = mesh.begin(entity,side_rank);
      stk::mesh::ConnectivityOrdinal const *existing_side_ordinals = mesh.begin_ordinals(entity, side_rank);
      int num_sides = mesh.num_connectivity(entity, side_rank);

      int probe = 0;
      for (;
          (probe < num_sides)
              && (!existing_sides[probe].is_local_offset_valid()
                  || (existing_side_ordinals[probe] != static_cast<unsigned>(side_ordinal)));
          ++probe);

      if ((probe >= num_sides) || !select_live(mesh.bucket(existing_sides[probe])) )
      {
        return false;
      }
    }
  }

  //check dead sides
  for (std::vector<entity_side>::const_iterator itr = dead_sides.begin();
      itr != dead_sides.end(); ++itr) {
    stk::mesh::Entity entity = mesh.get_entity(element_rank, itr->entity_id);
    //select the entity only if the current process in the owner
    // TODO fix the aura to correctly ghost the sides
    if (mesh.is_valid(entity) && mesh.parallel_owner_rank(entity) == mesh.parallel_rank()) {
    //if (entity != NULL) {
      //make sure the side exist
      int side_ordinal = itr->side_ordinal;

      //      stk::mesh::PairIterRelation existing_sides = mesh.relations(entity, side_rank);
      //
      //      for (; existing_sides.first != existing_sides.second &&
      //          existing_sides.first->relation_ordinal() != side_ordinal ;
      //          ++existing_sides.first);
      //
      //      //reached the end or side is not dead
      //      if (existing_sides.first == existing_sides.second
      //          || !select_dead( mesh.bucket(existing_sides.first->entity())) )
      //      {
      //        return false;
      //      }
      stk::mesh::Entity const* existing_sides = mesh.begin(entity,side_rank);
      stk::mesh::ConnectivityOrdinal const *existing_side_ordinals = mesh.begin_ordinals(entity, side_rank);
      int num_sides = mesh.num_connectivity(entity, side_rank);

      int probe = 0;
      for (;
          (probe < num_sides)
              && (!existing_sides[probe].is_local_offset_valid()
                  || (existing_side_ordinals[probe] != static_cast<unsigned>(side_ordinal)));
          ++probe);

      if ((probe >= num_sides) || !select_dead(mesh.bucket(existing_sides[probe])) )
      {
        return false;
      }
    }
  }

  return true;
}

//----------------------------------------------------------------------------------

//Validates that the correct entites were killed in this iteration
bool validate_iteration( stk::ParallelMachine pm, stk::mesh::fixtures::GridFixture & fixture, int iteration) {

  if (iteration >= NUM_ITERATIONS || iteration < 0) {
    return false;
  }

  stk::mesh::BulkData& mesh = fixture.bulk_data();
  stk::mesh::MetaData& fem_meta = fixture.fem_meta();

  stk::mesh::Part & dead_part = *fixture.dead_part();

  stk::mesh::Selector select_dead = dead_part & fem_meta.locally_owned_part();
  stk::mesh::Selector select_live = (!dead_part) & fem_meta.locally_owned_part();

  int num_dead[NUM_RANK] = {0, 0, 0, 0};
  int num_live[NUM_RANK] = {0, 0, 0, 0};

  for ( int i = 0; i < NUM_RANK ; ++i) {
    const stk::mesh::BucketVector& buckets = mesh.buckets( stk::mesh::EntityRank(i) );
    num_dead[i] = count_selected_entities( select_dead, buckets);
    num_live[i] = count_selected_entities( select_live, buckets);
  }

  stk::all_reduce(pm, stk::ReduceSum<NUM_RANK>(num_dead) & stk::ReduceSum<NUM_RANK>(num_live));

  bool correct_dead = true;
  bool correct_live = true;
  std::cout << std::endl;

  for (int i=0; i<NUM_RANK; ++i) {
    correct_dead &= global_num_dead[iteration][i] == num_dead[i];
    correct_live &= global_num_live[iteration][i] == num_live[i];
  }

  if (correct_dead && correct_live) {
    return validate_sides(fixture, iteration);
  }

  return false;
}
