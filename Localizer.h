#ifndef PGSLAM_LOCALIZER_H
#define PGSLAM_LOCALIZER_H

#include <deque>
#include <thread>
#include <condition_variable>

#include <Eigen/Geometry>

#include "types.h"
#include "MapManager.h"
#include "LocalMap.h"

namespace pgslam {

template<typename T>
class Localizer {
public:
  using MapManagerPtr = typename MapManager<T>::Ptr;

  IMPORT_PGSLAM_TYPES(T)

  using LocalMap = pgslam::LocalMap<T>;
  using LocalMapDataBuffer = typename pgslam::LocalMap<T>::DataBuffer;
  using LocalMapComposition = typename pgslam::LocalMap<T>::CompositionZ;

public:
  struct InputData {
    unsigned long long int timestamp;
    std::string world_frame_id;
    Matrix T_world_robot;
    Matrix T_robot_sensor;
    DPPtr cloud_ptr;
  };

public:
  Localizer(MapManagerPtr map_manager_ptr);
  ~Localizer();

  void SetLocalIcpConfig(const std::string &config_path);
  void SetInputFiltersConfig(const std::string &config_path);

  void AddNewData(const InputData &data);
  void Run();
  void Main();

  std::pair<DP,bool> GetLocalMap();
  std::pair<DP,bool> GetLocalMapInWorldFrame();

private:
  void ProcessFirstCloud(DPPtr cloud, const Matrix &T_world_robot);
  void UpdateBeforeIcp();
  void UpdateAfterIcp();

  void UpdateWorldRefkfPose(const Graph & g);
  void UpdateWorldRobotPose(const Graph & g);
  void UpdateLocalRobotPose(const Graph & g);
  bool HasEnoughOverlap(T overlap);
  bool IsBetterComposition(const LocalMapComposition comp);

  // T ComputeCurrentOverlap();
  // Matrix GetWorldRobotPose();
  // Matrix GetCurrentCovariance();

private:
  // Variables used to input data in the thread
  //! Variable used to stop the thread
  bool stop_ = {false};
  //! Buffer with new data to be processed
  std::deque<InputData> new_data_buffer_;
  //! Mutex to control access to new_data_buffer_
  std::mutex new_data_mutex_;
  //! Condition variable to inform localization thread of new data
  std::condition_variable new_data_cond_var_;
  //! Main thread object
  std::thread main_thread_;
  //! Variable used to store the cloud being currently processed by the thread
  DPPtr input_cloud_ptr_;

  // Variables used with data points
  //! Used to transform input clouds from sensor to robot frame
  TransformationPtr rigid_transformation_;
  //! Object that filters all input clouds
  DataPointsFilters input_filters_;
  //! ICP object
  ICPSequence icp_sequence_;

  //! Object to store shared data (graph of keyframes)
  MapManagerPtr map_manager_ptr_;

  // Variables used to store local and global robot poses
  //! Current reference keyframe at the world frame
  Matrix T_world_refkf_;
  //! Current robot pose at the current reference keyframe
  Matrix T_refkf_robot_;
  //! Current robot pose at world keyframe
  Matrix T_world_robot_;

  //! Last input robot pose at world keyframe, used to compute delta poses
  Matrix last_input_T_world_robot_;
  //! The graph vertex of the reference frame in the graph
  // Vertex refkf_vertex_;

  //! Buffer of vertexes that compose the next local map.
  LocalMapComposition next_local_map_composition_;
  //! Current local map structure. Contains a DP cloud.
  LocalMap local_map_;

  //! Min value for overlap range. Below this value the user is informed of potential problems or program stops with error
  T overlap_range_min_;
  //! Max value for overlap range. This is used to decide if local maps are good enough.
  T overlap_range_max_;

};

} // pgslam

#include "Localizer.hpp"

#endif // PGSLAM_LOCALIZER_H
