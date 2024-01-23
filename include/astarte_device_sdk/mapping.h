#ifndef AD_MAPPING_H
#define AD_MAPPING_H

#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief mapping type
 *
 * This enum represents the possible types of an Astarte interface mapping
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-mapping-schema-type
 */
typedef enum
{
    TYPE_INTEGER = 1,
    TYPE_LONGINTEGER,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BINARYBLOB,
    TYPE_BOOLEAN,
    TYPE_DATETIME,
    TYPE_INTEGERARRAY,
    TYPE_LONGINTEGERARRAY,
    TYPE_DOUBLEARRAY,
    TYPE_STRINGARRAY,
    TYPE_BINARYBLOBARRAY,
    TYPE_BOOLEANARRAY,
    TYPE_DATETIMEARRAY,
} ad_mapping_type_t;

/**
 * @brief mapping reliability definition
 *
 * This enum represents the possible values of the reliability of an astarte mapping
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-mapping-schema-reliability
 */
typedef enum
{
    RELIABILITY_UNRELIABLE = 0,
    RELIABILITY_GUARANTEED = 1,
    RELIABILITY_UNIQUE = 2,
} ad_mapping_reliability_t;

/**
 * @brief interface mapping definition
 *
 * This structs represent a subset of the information contained in an Astarte interface mapping
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#mapping
 */
typedef struct
{
    const char *endpoint;
    ad_mapping_type_t type;
    ad_mapping_reliability_t reliability;
    bool explicit_timestamp;
    bool allow_unset;
} ad_mapping_t;

/**
 * @brief interface mapping list embedded in the interface
 *
 * This structs represent the list of mapping associated to an interface with an associed length
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-mappings
 */
typedef struct
{
    const ad_mapping_t *buf; /**< the list of mappings */
    size_t len; /**< the length of the list of mappings */
} ad_mapping_list_t;

#endif // AD_MAPPING_H
