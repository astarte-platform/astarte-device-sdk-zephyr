#ifndef AD_INTROSPECTION_H
#define AD_INTROSPECTION_H

#include <astarte_device_sdk/interface.h>
#include <zephyr/sys/rb.h>
#include <zephyr/sys/util.h>

/**
 * @brief interface aggregation
 *
 * This enum represents the possible Astarte interface aggregation
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-aggregation
 */
typedef enum
{
    E_INTROSPECTION_OK = 1,
    E_INTROSPECTION_ALLOCATION, /**< Allocation error while reserving space for a node */
    E_INTROSPECTION_INT_ALREADY_PRESENT, /**< Interface is already present in the introspection */
    E_INTROSPECTION_INT_NOT_FOUND, /**< Interface not found */
} ad_introspection_res_t;

typedef struct
{
    struct rbtree tree;
} ad_introspection_t;

typedef struct
{
    const ad_interface_t *interface;
    struct rbnode node;
} ad_introspection_node_t;

// TODO add a local enum to identify errors

/**
 * @brief Initializes the introspection struct
 *
 * The resulting struct should be deallocated by the user by calling #ad_intospection_free
 */
ad_introspection_t ad_introspection_new();

/**
 * @brief Adds an interface to the introspection list
 *
 * @param introspection a pointer to an introspection struct initialized using #ad_introspection_new
 * @param interface the pointer to an interface struct
 */
ad_introspection_res_t ad_introspection_add(ad_introspection_t *introspection, const ad_interface_t *interface);

/**
 * @brief Retrieves an interface from the introspection list using the name as a key
 *
 * @param introspection a pointer to an introspection struct initialized using #ad_introspection_new
 * @param interface_name the name of one of the interfaces contained in the introspection list
 */
const ad_interface_t *ad_introspection_get_by_name(
    ad_introspection_t *introspection, char *interface_name);

/**
 * @brief Removes an interface from the introspection list
 *
 * @param introspection a pointer to an introspection struct initialized using #ad_introspection_new
 * @param interface_name the name of one of the interfaces contained in the introspection list
 */
ad_introspection_res_t ad_introspection_remove_by_name(ad_introspection_t *introspection, char *interface_name);

/**
 * @brief Returns the introspection string as described in astarte documentation
 *
 * An empty string is returned if not interfaces got added with #ad_introspection_add
 * https://docs.astarte-platform.org/astarte/latest/080-mqtt-v1-protocol.html#introspection
 *
 * @param introspection a pointer to an introspection struct initialized using #ad_introspection_new
 */
char *ad_introspection_get_string(ad_introspection_t *introspection);

/**
 * @brief Deallocates the introspection struct
 *
 * The struct gets initialized using #ad_introspection_new
 * @param introspection an owned introspection struct initialized using #ad_introspection_new
 */
void ad_introspection_free(ad_introspection_t introspection);

#endif // AD_INTROSPECTION_H
