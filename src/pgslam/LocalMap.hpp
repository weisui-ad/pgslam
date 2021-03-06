#ifndef PGSLAM_LOCAL_MAP_HPP
#define PGSLAM_LOCAL_MAP_HPP

#include "LocalMap.h"
#include "metrics.h"

#include <cassert>

namespace pgslam {

template<typename T>
LocalMap<T>::DataBuffer::DataBuffer()
 : boost::circular_buffer<LocalMap<T>::DataElement>()
{

}

template<typename T>
LocalMap<T>::DataBuffer::DataBuffer(size_t capacity)
 : boost::circular_buffer<LocalMap<T>::DataElement>(capacity)
{

}

template<typename T>
LocalMap<T>::DataBuffer::DataBuffer(const Graph & g, const Composition & comp)
{
  this->set_capacity(comp.capacity());
  for (auto v : comp)
    this->push_back(std::make_pair(v, g[v]));
}


template<typename T>
LocalMap<T>::LocalMap(size_t capacity)
 : data_{capacity},
   rigid_transformation_{PM::get().REG(Transformation).create("RigidTransformation")}
{}

template<typename T>
LocalMap<T>::LocalMap(const Graph & g, const Composition & comp)
 : data_{g, comp},
   rigid_transformation_{PM::get().REG(Transformation).create("RigidTransformation")}
{
  BuildCloudFromData();
}

template<typename T>
size_t LocalMap<T>::Capacity()
{
  return data_.capacity();
}

template<typename T>
void LocalMap<T>::UpdateFromGraph(const Graph & g)
{
  for (auto & e : data_)
    e.second = g[e.first];

  BuildCloudFromData();
}

template<typename T>
void LocalMap<T>::UpdateFromDataBuffer(const DataBuffer & data)
{
  data_ = data;

  BuildCloudFromData();
}

template<typename T>
void LocalMap<T>::UpdateToNewComposition(const Graph & g, const Composition & comp)
{
  data_.clear();
  data_.set_capacity(comp.capacity());
  for (auto v : comp)
    data_.push_back(std::make_pair(v, g[v]));

  BuildCloudFromData();
}

template<typename T>
bool LocalMap<T>::HasCloud() const
{
  return (cloud_.features.cols() != 0);
}

template<typename T>
const typename LocalMap<T>::DP & LocalMap<T>::Cloud() const
{
  return cloud_;
}

template<typename T>
typename LocalMap<T>::DP LocalMap<T>::CloudInWorldFrame() const
{
  return rigid_transformation_->compute(cloud_, ReferenceKeyframe().optimized_T_world_kf);
}

template<typename T>
typename LocalMap<T>::Composition LocalMap<T>::GetComposition() const
{
  Composition comp{data_.capacity()};
  for (auto & e : data_)
    comp.push_back(e.first);

  return std::move(comp);
}

template<typename T>
typename LocalMap<T>::Vertex LocalMap<T>::ReferenceVertex() const
{
  return data_.back().first;
}

template<typename T>
const typename LocalMap<T>::Keyframe & LocalMap<T>::ReferenceKeyframe() const
{
  return data_.back().second;
}

template<typename T>
bool LocalMap<T>::HasSameVertexSet(const Composition & comp) const
{
  // Since both should be unique we can test the size
  if (data_.size() != comp.size())
    return false;

  // Search for data element vertices on composition vertices
  auto comp_it = comp.begin();
  auto comp_it_end = comp.end();
  for(auto & elem : data_) {
    auto res = std::find_if(comp_it, comp_it_end, [&elem](auto & v) -> bool {
      return elem.first == v;
    });
    if (res == comp_it_end)
      return false; // Could not find data element vertex into composition
  }

  // Search for composition vertices on data elements
  auto data_it = data_.begin();
  auto data_it_end = data_.end();
  for(auto & v : comp) {
    auto res = std::find_if(data_it, data_it_end, [&v](auto & elem) -> bool {
      return elem.first == v;
    });
    if (res == data_it_end)
      return false; // Could not find composition vertex into data element
  }

  // Found all vertices in each other
  return true;
}

template<typename T>
bool LocalMap<T>::HasSameReferenceVertex(const Composition & comp) const
{
  // Reference keyframe is the last element of the circular buffer
  return data_.back().first == comp.back();
}

template<typename T>
bool LocalMap<T>::HasSameComposition(const Composition & comp) const
{
  return (HasSameReferenceVertex(comp) and HasSameVertexSet(comp));
}

template<typename T>
bool LocalMap<T>::IsOutdated(const Graph & g) const
{
  for (auto & element : data_)
    if (g[element.first].update_time > element.second.update_time)
      return true; // At least one vertex in the graph is more recent then one in this

  return false; // All vertices in this are up-to-date with other
}

template<typename T>
bool LocalMap<T>::IsReferenceKeyframeOutdated(const Graph & g) const
{
  // Refkf on the graph is more recent than reference keyframe on data_
  return (g[data_.back().first].update_time > data_.back().second.update_time);
}

template<typename T>
typename LocalMap<T>::Vertex LocalMap<T>::FindClosestVertex(const Matrix & T_world_x) const
{
  auto it = data_.begin();

  // Compute distance for the first vertex
  Vertex closest_v = it->first;
  T closest_dist = Metrics<T>::Distance(it->second.optimized_T_world_kf, T_world_x);

  // Find the closest vertex
  it++;
  std::for_each(it, data_.end(), [&T_world_x, &closest_v, &closest_dist](auto & e){
    T dist = Metrics<T>::Distance(e.second.optimized_T_world_kf, T_world_x);
    if (dist < closest_dist) {
      closest_v = e.first;
      closest_dist = dist;
    }
  });

  return closest_v;
}


template<typename T>
void LocalMap<T>::BuildCloudFromData()
{
  // Add reference kf cloud
  // NOTE: kf on the back is the reference kf
  auto rit = data_.rbegin();
  cloud_ = *(rit->second.cloud_ptr);
  // Get world transform in the reference kf, used below to convert other kf clouds
  Matrix T_refkf_world = rit->second.optimized_T_world_kf.inverse();
  rit++;

  // Convert all other kf clouds in the local map to refkf frame and
  // concatenate in the local map cloud
  std::for_each(rit, data_.rend(), [this, &T_refkf_world](const auto &e) {
    this->cloud_.concatenate(this->rigid_transformation_->compute(*(e.second.cloud_ptr), T_refkf_world * e.second.optimized_T_world_kf));
  });
}

} // pgslam

#endif // PGSLAM_LOCAL_MAP_HPP
