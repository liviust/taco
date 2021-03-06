#ifndef TACO_FORMAT_H
#define TACO_FORMAT_H

#include <string>
#include <memory>
#include <vector>
#include <ostream>

namespace taco {
class Level;

enum DimensionType {
  Dense,      // e.g. first  dimension in CSR
  Sparse,     // e.g. second dimension in CSR
  Fixed       // e.g. second dimension in ELL
};

class Format {
public:
  /// Create a format for a tensor with no dimensions
  Format();

  /// Create a tensor format that can be used with any tensor and whose
  /// dimensions have the same storage type.
  Format(const DimensionType& dimensionType);

  /// Create a tensor format where the dimensions have the given storage types.
  /// The dimensions are ordered from first to last.
  Format(const std::vector<DimensionType>& dimensionTypes);

  /// Create a tensor format where the dimensions have the given storage types and
  /// dimension order.
  Format(const std::vector<DimensionType>& dimensionTypes,
         const std::vector<int>& dimensionOrder);

  /// Returns the number of dimensions in the format.
  size_t getOrder() const;

  /// Get the storage types of the dimensions.
  const std::vector<DimensionType>& getDimensionTypes() const;

  /// Get the storage order of the dimensions. The storage order is a
  /// permutation vector where location i contains the storage location of
  /// dimension i.
  const std::vector<int>& getDimensionOrder() const;

  /// Get the tensor storage levels.
  const std::vector<Level>& getLevels() const {return levels;}

  // True if all dimensions are Dense
  bool isDense() const;

private:
  std::vector<Level> levels;

  std::vector<DimensionType> dimensionTypes;
  std::vector<int> dimensionOrder;
};

bool operator==(const Format&, const Format&);
bool operator!=(const Format&, const Format&);

std::ostream &operator<<(std::ostream&, const Format&);


class Level {
public:
  Level(size_t dimension, DimensionType type) : dimension(dimension), type(type) {}

  DimensionType getType() const {
    return type;
  }

  size_t getDimension() const {
    return dimension;
  }

private:
  size_t dimension;  // The tensor dimension described by the format level
  DimensionType type;
};

std::ostream& operator<<(std::ostream&, const DimensionType&);
std::ostream& operator<<(std::ostream&, const Level&);


// Predefined formats
extern const Format CSR;
extern const Format CSC;
extern const Format DCSR;
extern const Format DCSC;

}
#endif
