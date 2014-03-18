/**
 * @file point.h
 * @author Chase Geigle
 *
 * All files in META are released under the MIT license. For more details,
 * consult the file LICENSE in the root of the project.
 */

#ifndef META_CLUSTER_CENTER_H_
#define META_CLUSTER_CENTER_H_

#include <cstddef>
#include <unordered_map>

#include "cluster/similarity.h"
#include "corpus/document.h"

namespace meta
{
namespace clustering
{

/**
 * Represents a point in high-dimensional space to be used in clustering.
 */
template <class DimensionKey, class Element>
class point
{
  public:
    using sparse_vector = std::unordered_map<DimensionKey, double>;

    // specialize this to add support for new data types to be converted
    // to points
    point(const Element& elem);

    point(const sparse_vector& vec, size_t size)
        : element_{nullptr}, size_{size}, avg_vector_{vec}
    {
    }

    const sparse_vector& vector() const
    {
        return avg_vector_;
    }

    size_t size() const
    {
        return size_;
    }

    const Element* element() const
    {
        return element_;
    }

  private:
    const Element* element_; // only exists for points which actually have
                             // a single element (e.g. leaves of a
                             // hierarchical cluster)
    size_t size_;
    sparse_vector avg_vector_;
};

template <>
point<term_id, corpus::document>::point(const corpus::document& d)
    : element_{&d}, size_{1}
{
    for (const auto& freq : d.frequencies())
        avg_vector_[freq.first] = static_cast<double>(freq.second);
}

point<term_id, corpus::document> make_point(const corpus::document& d)
{
    return point<term_id, corpus::document>{d};
}

template <class DimensionKey, class Element>
point<DimensionKey, Element>
    merge_points(const point<DimensionKey, Element>& first,
                 const point<DimensionKey, Element>& second)
{

    using namespace similarity::internal;

    typename point<DimensionKey, Element>::sparse_vector avg_vector;
    for (const auto& key : get_space(first.vector(), second.vector()))
    {
        auto first_contrib = first.size() * safe_at(first.vector(), key);
        auto second_contrib = second.size() * safe_at(second.vector(), key);
        avg_vector[key] = (first_contrib + second_contrib)
                          / (first.size() + second.size());
    }

    return {avg_vector, first.size() + second.size()};
}
}
}

#endif