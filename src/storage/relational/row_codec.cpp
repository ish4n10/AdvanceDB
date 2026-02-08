#include "storage/relational/row_codec.hpp"

using namespace Relational;

RowCodec::RowCodec(const TableSchema& _schema) : schema(_schema) {
    
}

std::vector<uint8_t> RowCodec::encode(const Tuple& tuple) const {
    
}