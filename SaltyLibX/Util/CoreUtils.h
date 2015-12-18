// Random general purpose utility functions


#define DEFINE_ENUM_BITWISE_OPERANDS(EnumType) \
inline EnumType operator&(EnumType field1, EnumType field2) \
{ return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(field1) & static_cast<std::underlying_type<EnumType>::type>(field2)); } \
	\
inline EnumType operator|(EnumType field1, EnumType field2) \
{ return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(field1) | static_cast<std::underlying_type<EnumType>::type>(field2)); } \
	\
inline EnumType& operator&=(EnumType& field1, EnumType field2) \
{ 	field1 = (field1 & field2); return field1; } \
inline EnumType& operator|=(EnumType& field1, EnumType field2) \
{ 	field1 = (field1 | field2); return field1; } \

//inline EnumType operator&(EnumType field1, EnumType field2) \
//{ return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>(field1) & static_cast<std::underlying_type<EnumType>::type>>(field2)); } \
//	\
//inline EnumType operator|(EnumType field1, EnumType field2) \
//{ return static_cast<EnumType>(static_cast<std::underlying_type<EnumType>::type>>(field1) | static_cast<std::underlying_type<EnumType>::type>(field2)); }
