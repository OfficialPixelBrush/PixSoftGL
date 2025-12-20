#define MAX_VERTICES 64

#define MAX_LIGHTS 8
#define MAX_CLIP_PLANES 6
#define MAX_MODELVIEW_STACK_DEPTH 32
#define MAX_PROJECTION_STACK_DEPTH 2
#define MAX_TEXTURE_STACK_DEPTH 32
#define MAX_TEXTURE_SIZE 64

#define CODE_MAX_TEXTURE_SIZE 0x0D33

enum PacketType {
  PacketNone,
  PacketBegin,
  PacketEnd,
  PacketVertex,
  PacketColor,
  PacketUV,
  PacketGet,
};

enum DataType {
  TypeBool,
  TypeByte,
  TypeShort,

  TypeInteger,
  TypeInteger2,
  TypeInteger3,
  TypeInteger4,

  TypeFloat,
  TypeFloat2,
  TypeFloat3,
  TypeFloat4,

  TypeDouble,
  TypeDouble2,
  TypeDouble3,
  TypeDouble4
};

struct BasePacket {
  PacketType ptype = PacketNone;
};

struct ValuePacket : public BasePacket {
  DataType dtype;
};

// Single-value packets
struct BytePacket : public ValuePacket {
  char value;
};

struct ShortPacket : public ValuePacket {
  short value;
};

struct IntegerPacket : public ValuePacket {
  int value;
};

struct FloatPacket : public ValuePacket {
  float value;
};

struct DoublePacket : public ValuePacket {
  double value;
};

// Multi-byte packets
struct Float2fPacket : public ValuePacket {
  float x,y;
};

struct Float3fPacket : public ValuePacket {
  float x,y,z;
};

struct Float4fPacket : public ValuePacket {
  float x,y,z,w;
};

// Active packet and data type
byte packetType = PacketNone;
byte dataType = TypeInteger;

void setup() {
  Serial.begin(115200);
}

// Basic communication
byte ReadByte() {
  while(Serial.available() < sizeof(byte)) {}
  byte value = 0;
  Serial.readBytes(&value, sizeof(byte));
  return value;
}

void WriteByte(byte value) {
  Serial.write(value);
}

short ReadShort() {
  while(Serial.available() < sizeof(short)) {}
  short value = 0;
  Serial.readBytes((uint8_t*)&value, sizeof(short));
  return value;
}

void WriteShort(short value) {
  Serial.write(value);
}

int ReadInteger() {
  while(Serial.available() < sizeof(int)) {}
  int value = 0;
  Serial.readBytes((uint8_t*)&value, sizeof(int));
  return value;
}

void WriteInteger(int value) {
  Serial.write(value);
}

float ReadFloat() {
  while(Serial.available() < sizeof(float)) {}
  float value = 0.0f;
  Serial.readBytes((uint8_t*)&value, sizeof(float));
  return value;
}

void WriteFloat(float value) {
  //Serial.write(value);
}

double ReadDouble() {
  while(Serial.available() < sizeof(double)) {}
  double value = 0.0;
  Serial.readBytes((uint8_t*)&value, sizeof(double));
  return value;
}

void WriteDouble(double value) {
  //Serial.write(value);
}

void SendFramebuffer() {
  // Serial.write()
}

// Reading of packet types
void ReadPacketType() {
  packetType = ReadByte();
}

void ReadDataType() {
  dataType = ReadByte();
}

struct Vec4 {
  float x,y,z,w;
};

struct Col4 {
  float r,g,b,a;
};

struct Vec2 {
  float x,y;
};

struct Vertex {
  Vec4 pos = Vec4{0,0,0,0};
  Col4 col = Col4{1,1,1,1};
  Vec2 uv = Vec2{0,0};
};

int vertPtr = 0;
Vertex vertices[MAX_VERTICES];

struct Vec4 ReadMultiFloat() {
  Vec4 v;
  switch (dataType) {
    case TypeFloat4:
      v.w = ReadFloat();
    case TypeFloat3:
      v.z = ReadFloat();
    case TypeFloat2:
      v.y = ReadFloat();
    case TypeFloat:
      v.x = ReadFloat();
      break;
  }
  return v;
}

struct Col4 ReadMultiFloatCol() {
  Vec4 v = ReadMultiFloat();
  return Col4{v.x,v.y,v.z,v.w};
}

Col4 activeColor = Col4{0,0,0,1};
Vec2 activeUV = Vec2{0,0};

void loop() {
  // send data only when you receive data:

  // If we received nothing, do nothing
  if (Serial.available() <= 0) return;

  ReadPacketType();
  switch(packetType) {
    // Request values from server
    case PacketGet: {
      short value = ReadShort();
      switch (value) {
        case CODE_MAX_TEXTURE_SIZE:
          WriteInteger(MAX_TEXTURE_SIZE);
          break;
      }
      break;
      }
    // Receive Vertex from Client
    case PacketVertex: {
      ReadDataType();
      Vertex v;
      v.pos = ReadMultiFloat();
      v.col = activeColor;
      v.uv = activeUV;
      vertices[vertPtr++] = v;
      break;
    }
    case PacketColor: {
      ReadDataType();
      activeColor = ReadMultiFloatCol();
      break;
    }
    case PacketUV: {
      ReadDataType();
      Vec4 mf = ReadMultiFloat();
      activeUV = Vec2{mf.x,mf.y};
      break;
    }
    default:
      break;
  }
}
