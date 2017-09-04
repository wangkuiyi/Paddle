## Background

PaddlePaddle distinguishes the description of a neural networks as *compiling* and the execution as *running*.  At *compile time*, users' Python program executes to build the computation graph.  At *runtime*, PaddlePaddle runs the built computation graph.

We'd need to serialize/deserialize compile results for distributed training and enterprise deployment, so we use protobuf message to describe the graph -- `OpDesc` for operators and `VarDesc` for variables.

These protobuf messages allow the inference of the size of operators' outputs according to input sizes.  Such size information is saved in `VarDesc`.

`VarDesc` and `OpDesc` together describes computation graphs, basing on which, we can reuse variables as much as we could.

PaddlePaddle variables, like a C++/Java variables, have the type property.   The type could be Tensor, scalar values, and Scope.

## Definition of VarDesc in Proto

```
message LoDTensorDesc {
  enum Type {
    INT16 = 1;
    INT32 = 2;
    INT64 = 3;
    FP16 = 4;
    FP32 = 5;
    DOUBLE = 6
    BOOL = 7;
  }

  Type element_type = 1;
  repeated int dims = 2; // [UNK, UNK, 6000] is saved as [-1, -1, 6000]
  optional int lod_level [default=0] = 3;
  repeated int32 int16_val = 4 [packed = true]; // INT16
  repeated int32 int32_val = 5 [packed = true]; // INT32
  repeated int64 int64_val = 6 [packed = true]; // INT64
  repeated float float_val = 7 [packed = true]; // FP32
  repeated double double_val = 8 [packed = true]; // DOUBLE
  repeated bool bool_val = 9 [packed = true];  // BOOL
}

message VarDesc {
  enum Type {
    INT = 0;
    FLOAT = 1;
    STRING = 2;
    INTS = 3;
    FLOATS = 4;
    STRINGS = 5;
    LOD_TENSOR = 6;
  }

  message Value {
    optional int32 i = 1;
    optional float f = 2;
    optional string s = 3;
    repeated int32 ints = 4;
    repeated float floats = 5;
    repeated string strings = 6;
    optional LodTesnorDesc lod_tensor = 7; // when type==LOD_TENSOR
  }

  required string name = 1;
  required Type type = 2;
  required Value value = 3;
}

```

## Definition of Variable in Python

There is a class `Variable` in python to help create Variable.

```python
class Variable(object):
  def __init__(self,
               name=None,
               data_type=None,
               shape=None,
               value=None,
               trainable=True):
```

create a variable with a tensor value.

```python
a = Variable("X", shape=[784, 10], data_type=pd.INT32, value=0)
```

or create a Variable with a string value

```python
a = Variable("X", data_type=pd.STRING, value="aa")
```
