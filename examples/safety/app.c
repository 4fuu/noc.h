#include <stdlib.h>

typedef struct {
    int value;
} Resource;

static int cleanup_order;
static int drop_count;

static void record_cleanup(int value)
{
    cleanup_order = cleanup_order * 10 + value;
}

static Resource *acquire_resource(int value)
{
    Resource *resource = (Resource *)malloc(sizeof(*resource));
    if (resource) resource->value = value;
    return resource;
}

static void release_resource(Resource *resource)
{
    drop_count += 1;
    free(resource);
}

static void observe_resource(Resource *resource)
{
    if (resource) cleanup_order += resource->value;
}

/* Templates are explicitly instantiated, so generated C has ordinary names. */
template(T, identity)
T identity(T value)
{
    return value;
}

instantiate(identity, int, identity_int);

static int deferred_value(void)
{
    int value = 7;
    /* The return expression is evaluated before this cleanup runs. */
    defer value = 0;
    return value;
}

static void deferred_order(void)
{
    defer record_cleanup(1);
    defer record_cleanup(2);
}

static void local_owner(void)
{
    /* borrow is non-consuming; lexical exit drops this owner exactly once. */
    own(Resource *, release_resource) resource = acquire_resource(3);
    observe_resource(borrow(resource));
}

static Resource *transferred_owner(void)
{
    own(Resource *, release_resource) first = acquire_resource(42);
    own(Resource *, release_resource) second = move(first);
    /* Explicit return transfer prevents either local owner from dropping it. */
    return move(second);
}

int main(void)
{
    Resource *transferred;
    if (identity_int(5) != 5 || deferred_value() != 7) return 1;
    deferred_order();
    if (cleanup_order != 21) return 2;
    local_owner();
    if (drop_count != 1 || cleanup_order != 24) return 3;
    transferred = transferred_owner();
    if (!transferred || transferred->value != 42 || drop_count != 1) return 4;
    release_resource(transferred);
    return drop_count == 2 ? 0 : 5;
}
