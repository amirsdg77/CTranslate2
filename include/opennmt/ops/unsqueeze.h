#pragma once

#include "opennmt/ops/op.h"

namespace opennmt {
  namespace ops {

    class Unsqueeze : public UnaryOp {
    public:
      Unsqueeze(const std::vector<size_t>& axes)
        : _axes(axes) {
        std::sort(_axes.begin(), _axes.end());
      }

      void operator()(StorageView& data) const {
        data.reshape(transform_shape(data.shape()));
      }
      void operator()(const StorageView& data, StorageView& expanded) const override {
        expanded.shallow_copy(const_cast<StorageView&>(data));
        expanded.reshape(transform_shape(data.shape()));
      }

    private:
      std::vector<size_t> _axes;

      Shape transform_shape(const Shape& shape) const {
        Shape new_shape;
        for (size_t i = 0, j = 0; i < shape.size(); ++i) {
          if (j < _axes.size() && i == _axes[j]) {
            ++j;
            new_shape.push_back(1);
          }
          new_shape.push_back(shape[i]);
        }
        return new_shape;
      }
    };

  }
}
