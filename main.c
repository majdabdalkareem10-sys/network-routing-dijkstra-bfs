#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROUTERS 100
#define LINE_LENGTH 256

#define ROUTERS_FILE "routers.txt"
#define OUTPUT_FILE "shortest_distance.txt"

#define NO_EDGE INT_MAX

/*
 * Network Packet Routing System
 *
 * Graph representation: Adjacency Matrix
 * Shortest latency path: Dijkstra using a Min Heap
 * Minimum-hop path: BFS using a Queue
 *
 * Original university Data Structures project,
 * later reviewed and refactored for portfolio documentation.
 */

/* ============================= Structures ============================= */

typedef struct {
    char routers[MAX_ROUTERS];
    int adjacency[MAX_ROUTERS][MAX_ROUTERS];
    int router_count;
    bool loaded;
} Graph;

typedef struct {
    int vertex;
    int distance;
} HeapNode;

typedef struct {
    HeapNode *nodes;
    int size;
    int capacity;
} MinHeap;

typedef struct {
    int items[MAX_ROUTERS];
    int front;
    int rear;
} Queue;

typedef struct {
    bool available;

    char source;
    char destination;

    bool dijkstra_reachable;
    int dijkstra_path[MAX_ROUTERS];
    int dijkstra_length;
    int dijkstra_cost;

    bool bfs_reachable;
    int bfs_path[MAX_ROUTERS];
    int bfs_length;
    int bfs_cost;
} RouteResult;

/* ============================= Utilities ============================= */

static void fatal_error(const char *message) {
    fprintf(stderr, "Fatal error: %s\n", message);
    exit(EXIT_FAILURE);
}

static void *safe_malloc(size_t size) {
    void *pointer = malloc(size);

    if (pointer == NULL) {
        fatal_error("memory allocation failed");
    }

    return pointer;
}

static char *trim(char *text) {
    while (isspace((unsigned char)*text)) {
        ++text;
    }

    if (*text == '\0') {
        return text;
    }

    char *end = text + strlen(text) - 1;

    while (
        end > text &&
        isspace((unsigned char)*end)
    ) {
        --end;
    }

    end[1] = '\0';

    return text;
}

static bool read_line(
    const char *prompt,
    char *buffer,
    size_t size
) {
    if (prompt != NULL) {
        printf("%s", prompt);
    }

    if (fgets(buffer, (int)size, stdin) == NULL) {
        return false;
    }

    size_t length = strlen(buffer);

    if (
        length > 0 &&
        buffer[length - 1] == '\n'
    ) {
        buffer[length - 1] = '\0';
    } else {
        int character;

        while (
            (character = getchar()) != '\n' &&
            character != EOF
        ) {
        }
    }

    char *clean = trim(buffer);

    if (clean != buffer) {
        memmove(
            buffer,
            clean,
            strlen(clean) + 1
        );
    }

    return true;
}

static bool read_integer(
    const char *prompt,
    int *value
) {
    char line[LINE_LENGTH];

    while (true) {
        if (
            !read_line(
                prompt,
                line,
                sizeof(line)
            )
        ) {
            return false;
        }

        char *end = NULL;

        long parsed =
            strtol(line, &end, 10);

        while (
            end != NULL &&
            isspace((unsigned char)*end)
        ) {
            ++end;
        }

        if (
            line != end &&
            end != NULL &&
            *end == '\0' &&
            parsed >= INT_MIN &&
            parsed <= INT_MAX
        ) {
            *value = (int)parsed;

            return true;
        }

        printf("Please enter a valid integer.\n");
    }
}

static bool read_router(
    const char *prompt,
    char *router
) {
    char line[LINE_LENGTH];

    while (true) {
        if (
            !read_line(
                prompt,
                line,
                sizeof(line)
            )
        ) {
            return false;
        }

        if (
            strlen(line) == 1 &&
            !isspace((unsigned char)line[0])
        ) {
            *router = line[0];

            return true;
        }

        printf(
            "Please enter exactly one "
            "router character.\n"
        );
    }
}

/* =============================== Graph =============================== */

static void graph_initialize(Graph *graph) {
    graph->router_count = 0;
    graph->loaded = false;

    for (
        int row = 0;
        row < MAX_ROUTERS;
        ++row
    ) {
        for (
            int column = 0;
            column < MAX_ROUTERS;
            ++column
        ) {
            graph->adjacency[row][column] =
                row == column
                    ? 0
                    : NO_EDGE;
        }
    }
}

static int graph_find_router(
    const Graph *graph,
    char router
) {
    for (
        int index = 0;
        index < graph->router_count;
        ++index
    ) {
        if (graph->routers[index] == router) {
            return index;
        }
    }

    return -1;
}

static int graph_add_router(
    Graph *graph,
    char router
) {
    int existing =
        graph_find_router(
            graph,
            router
        );

    if (existing != -1) {
        return existing;
    }

    if (
        graph->router_count >=
        MAX_ROUTERS
    ) {
        return -1;
    }

    graph->routers[graph->router_count] =
        router;

    return graph->router_count++;
}

static bool parse_edge(
    const char *line,
    char *from,
    char *to,
    int *latency
) {
    char extra;

    int matched = sscanf(
        line,
        " %c - %c - %d %c",
        from,
        to,
        latency,
        &extra
    );

    return matched == 3;
}

static bool graph_load(
    Graph *graph,
    const char *filename
) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror(filename);
        return false;
    }

    graph_initialize(graph);

    char line[LINE_LENGTH];

    int line_number = 0;
    int edge_count = 0;

    while (
        fgets(
            line,
            sizeof(line),
            file
        ) != NULL
    ) {
        ++line_number;

        char *clean = trim(line);

        if (*clean == '\0') {
            continue;
        }

        if (
            clean[0] == '/' &&
            clean[1] == '/'
        ) {
            continue;
        }

        char from;
        char to;
        int latency;

        if (
            !parse_edge(
                clean,
                &from,
                &to,
                &latency
            ) ||
            latency < 0
        ) {
            fprintf(
                stderr,
                "Warning: ignored invalid edge "
                "on line %d.\n",
                line_number
            );

            continue;
        }

        int from_index =
            graph_add_router(
                graph,
                from
            );

        int to_index =
            graph_add_router(
                graph,
                to
            );

        if (
            from_index == -1 ||
            to_index == -1
        ) {
            fclose(file);

            fprintf(
                stderr,
                "Error: graph exceeds "
                "%d routers.\n",
                MAX_ROUTERS
            );

            graph_initialize(graph);

            return false;
        }

        if (from_index == to_index) {
            continue;
        }

        /*
         * If the same link appears more than once,
         * store the smallest latency.
         */
        if (
            graph->adjacency
                [from_index][to_index]
                == NO_EDGE ||
            latency <
            graph->adjacency
                [from_index][to_index]
        ) {
            graph->adjacency
                [from_index][to_index] =
                latency;

            graph->adjacency
                [to_index][from_index] =
                latency;
        }

        ++edge_count;
    }

    fclose(file);

    if (
        graph->router_count == 0 ||
        edge_count == 0
    ) {
        fprintf(
            stderr,
            "Error: no valid router links "
            "were loaded.\n"
        );

        graph_initialize(graph);

        return false;
    }

    graph->loaded = true;

    printf(
        "Loaded %d router(s) and "
        "%d link(s) from %s.\n",
        graph->router_count,
        edge_count,
        filename
    );

    return true;
}

/* ============================== Min Heap ============================== */

static void heap_initialize(
    MinHeap *heap
) {
    heap->size = 0;
    heap->capacity = 16;

    heap->nodes = safe_malloc(
        (size_t)heap->capacity *
        sizeof(*heap->nodes)
    );
}

static void heap_destroy(
    MinHeap *heap
) {
    free(heap->nodes);

    heap->nodes = NULL;
    heap->size = 0;
    heap->capacity = 0;
}

static bool heap_is_empty(
    const MinHeap *heap
) {
    return heap->size == 0;
}

static bool heap_node_less(
    HeapNode first,
    HeapNode second
) {
    if (
        first.distance !=
        second.distance
    ) {
        return
            first.distance <
            second.distance;
    }

    return first.vertex < second.vertex;
}

static void heap_swap(
    HeapNode *first,
    HeapNode *second
) {
    HeapNode temporary = *first;

    *first = *second;
    *second = temporary;
}

static void heap_push(
    MinHeap *heap,
    int vertex,
    int distance
) {
    if (
        heap->size ==
        heap->capacity
    ) {
        int new_capacity =
            heap->capacity * 2;

        HeapNode *new_nodes =
            realloc(
                heap->nodes,
                (size_t)new_capacity *
                sizeof(*heap->nodes)
            );

        if (new_nodes == NULL) {
            heap_destroy(heap);

            fatal_error(
                "memory allocation failed "
                "while growing the heap"
            );
        }

        heap->nodes = new_nodes;
        heap->capacity = new_capacity;
    }

    int index = heap->size++;

    heap->nodes[index].vertex = vertex;
    heap->nodes[index].distance = distance;

    while (index > 0) {
        int parent =
            (index - 1) / 2;

        if (
            !heap_node_less(
                heap->nodes[index],
                heap->nodes[parent]
            )
        ) {
            break;
        }

        heap_swap(
            &heap->nodes[index],
            &heap->nodes[parent]
        );

        index = parent;
    }
}

static HeapNode heap_pop(
    MinHeap *heap
) {
    if (heap_is_empty(heap)) {
        fatal_error(
            "attempted to remove "
            "from an empty heap"
        );
    }

    HeapNode minimum =
        heap->nodes[0];

    --heap->size;

    if (heap->size > 0) {
        heap->nodes[0] =
            heap->nodes[heap->size];
    }

    int index = 0;

    while (index < heap->size) {
        int left =
            2 * index + 1;

        int right =
            2 * index + 2;

        int smallest = index;

        if (
            left < heap->size &&
            heap_node_less(
                heap->nodes[left],
                heap->nodes[smallest]
            )
        ) {
            smallest = left;
        }

        if (
            right < heap->size &&
            heap_node_less(
                heap->nodes[right],
                heap->nodes[smallest]
            )
        ) {
            smallest = right;
        }

        if (smallest == index) {
            break;
        }

        heap_swap(
            &heap->nodes[index],
            &heap->nodes[smallest]
        );

        index = smallest;
    }

    return minimum;
}

/* ================================ Queue ================================ */

static void queue_initialize(
    Queue *queue
) {
    queue->front = 0;
    queue->rear = 0;
}

static bool queue_is_empty(
    const Queue *queue
) {
    return
        queue->front ==
        queue->rear;
}

static void queue_enqueue(
    Queue *queue,
    int value
) {
    if (
        queue->rear >=
        MAX_ROUTERS
    ) {
        fatal_error(
            "queue capacity exceeded"
        );
    }

    queue->items[queue->rear++] =
        value;
}

static int queue_dequeue(
    Queue *queue
) {
    if (queue_is_empty(queue)) {
        fatal_error(
            "attempted to remove "
            "from an empty queue"
        );
    }

    return
        queue->items[
            queue->front++
        ];
}

/* ============================ Path Functions ============================ */

static bool build_path(
    int source,
    int destination,
    const int previous[MAX_ROUTERS],
    int path[MAX_ROUTERS],
    int *path_length
) {
    int reversed[MAX_ROUTERS];
    int length = 0;

    int current = destination;

    while (
        current != -1 &&
        length < MAX_ROUTERS
    ) {
        reversed[length++] = current;

        if (current == source) {
            break;
        }

        current = previous[current];
    }

    if (
        length == 0 ||
        reversed[length - 1] != source
    ) {
        *path_length = 0;

        return false;
    }

    *path_length = length;

    for (
        int index = 0;
        index < length;
        ++index
    ) {
        path[index] =
            reversed[length - 1 - index];
    }

    return true;
}

/* =============================== Dijkstra =============================== */

static bool dijkstra_shortest_path(
    const Graph *graph,
    int source,
    int destination,
    int path[MAX_ROUTERS],
    int *path_length,
    int *total_cost
) {
    int distance[MAX_ROUTERS];
    int previous[MAX_ROUTERS];

    for (
        int index = 0;
        index < graph->router_count;
        ++index
    ) {
        distance[index] = NO_EDGE;
        previous[index] = -1;
    }

    MinHeap heap;

    heap_initialize(&heap);

    distance[source] = 0;

    heap_push(
        &heap,
        source,
        0
    );

    while (!heap_is_empty(&heap)) {
        HeapNode current_node =
            heap_pop(&heap);

        int current =
            current_node.vertex;

        /*
         * Ignore old heap entries that no longer
         * represent the best known distance.
         */
        if (
            current_node.distance !=
            distance[current]
        ) {
            continue;
        }

        if (current == destination) {
            break;
        }

        for (
            int neighbor = 0;
            neighbor <
                graph->router_count;
            ++neighbor
        ) {
            int weight =
                graph->adjacency
                    [current][neighbor];

            if (
                weight == NO_EDGE ||
                current == neighbor
            ) {
                continue;
            }

            long long candidate =
                (long long)
                distance[current] +
                weight;

            if (
                candidate <
                    distance[neighbor] &&
                candidate <= INT_MAX
            ) {
                distance[neighbor] =
                    (int)candidate;

                previous[neighbor] =
                    current;

                heap_push(
                    &heap,
                    neighbor,
                    distance[neighbor]
                );
            }
        }
    }

    heap_destroy(&heap);

    if (
        distance[destination] ==
        NO_EDGE
    ) {
        *path_length = 0;

        return false;
    }

    *total_cost =
        distance[destination];

    return build_path(
        source,
        destination,
        previous,
        path,
        path_length
    );
}

/* ================================= BFS ================================= */

static bool bfs_shortest_path(
    const Graph *graph,
    int source,
    int destination,
    int path[MAX_ROUTERS],
    int *path_length,
    int *total_cost
) {
    bool visited[MAX_ROUTERS] = {false};
    int previous[MAX_ROUTERS];

    for (
        int index = 0;
        index < graph->router_count;
        ++index
    ) {
        previous[index] = -1;
    }

    Queue queue;

    queue_initialize(&queue);

    visited[source] = true;

    queue_enqueue(
        &queue,
        source
    );

    while (!queue_is_empty(&queue)) {
        int current =
            queue_dequeue(&queue);

        if (current == destination) {
            break;
        }

        for (
            int neighbor = 0;
            neighbor <
                graph->router_count;
            ++neighbor
        ) {
            if (
                !visited[neighbor] &&
                graph->adjacency
                    [current][neighbor]
                    != NO_EDGE &&
                current != neighbor
            ) {
                visited[neighbor] = true;

                previous[neighbor] =
                    current;

                queue_enqueue(
                    &queue,
                    neighbor
                );
            }
        }
    }

    if (!visited[destination]) {
        *path_length = 0;

        return false;
    }

    if (
        !build_path(
            source,
            destination,
            previous,
            path,
            path_length
        )
    ) {
        return false;
    }

    long long cost = 0;

    for (
        int index = 0;
        index + 1 < *path_length;
        ++index
    ) {
        cost +=
            graph->adjacency
                [path[index]]
                [path[index + 1]];
    }

    if (cost > INT_MAX) {
        return false;
    }

    *total_cost = (int)cost;

    return true;
}

/* =========================== Output Functions =========================== */

static void print_path(
    FILE *output,
    const Graph *graph,
    const int path[MAX_ROUTERS],
    int path_length
) {
    for (
        int index = 0;
        index < path_length;
        ++index
    ) {
        fprintf(
            output,
            "%c",
            graph->routers[path[index]]
        );

        if (
            index + 1 <
            path_length
        ) {
            fprintf(output, " -> ");
        }
    }
}

static void print_route_result(
    FILE *output,
    const Graph *graph,
    const RouteResult *result
) {
    fprintf(
        output,
        "Shortest path from %c to %c is:\n",
        result->source,
        result->destination
    );

    fprintf(output, "Dijkstra: ");

    if (result->dijkstra_reachable) {
        print_path(
            output,
            graph,
            result->dijkstra_path,
            result->dijkstra_length
        );

        fprintf(
            output,
            " with a total cost of %d.\n",
            result->dijkstra_cost
        );
    } else {
        fprintf(
            output,
            "no route exists.\n"
        );
    }

    fprintf(output, "BFS: ");

    if (result->bfs_reachable) {
        print_path(
            output,
            graph,
            result->bfs_path,
            result->bfs_length
        );

        fprintf(
            output,
            " with a total cost of %d.\n",
            result->bfs_cost
        );
    } else {
        fprintf(
            output,
            "no route exists.\n"
        );
    }
}

static bool calculate_routes(
    const Graph *graph,
    char source_router,
    char destination_router,
    RouteResult *result
) {
    int source =
        graph_find_router(
            graph,
            source_router
        );

    int destination =
        graph_find_router(
            graph,
            destination_router
        );

    if (
        source == -1 ||
        destination == -1
    ) {
        printf(
            "Source or destination router "
            "does not exist in the loaded graph.\n"
        );

        return false;
    }

    result->available = true;
    result->source = source_router;
    result->destination = destination_router;

    result->dijkstra_reachable =
        dijkstra_shortest_path(
            graph,
            source,
            destination,
            result->dijkstra_path,
            &result->dijkstra_length,
            &result->dijkstra_cost
        );

    result->bfs_reachable =
        bfs_shortest_path(
            graph,
            source,
            destination,
            result->bfs_path,
            &result->bfs_length,
            &result->bfs_cost
        );

    print_route_result(
        stdout,
        graph,
        result
    );

    return true;
}

static bool save_last_result(
    const Graph *graph,
    const RouteResult *result,
    const char *filename
) {
    if (!result->available) {
        printf(
            "No shortest-path result "
            "is available to save.\n"
        );

        return false;
    }

    FILE *file =
        fopen(filename, "w");

    if (file == NULL) {
        perror(filename);

        return false;
    }

    print_route_result(
        file,
        graph,
        result
    );

    fclose(file);

    printf(
        "Last route result saved "
        "to %s.\n",
        filename
    );

    return true;
}

/* ================================ Menu ================================ */

static void print_menu(void) {
    printf(
        "\n========== Network Routing Menu ==========\n"
        "1. Load routers\n"
        "2. Enter source router\n"
        "3. Enter destination and calculate paths\n"
        "4. Save the last result and exit\n"
        "==========================================\n"
    );
}

/* ================================ Main ================================ */

int main(void) {
    Graph graph;

    graph_initialize(&graph);

    RouteResult last_result = {0};

    char source_router = '\0';

    bool source_selected = false;

    while (true) {
        print_menu();

        int choice;

        if (
            !read_integer(
                "Choose an option: ",
                &choice
            )
        ) {
            printf("Input ended.\n");
            break;
        }

        switch (choice) {
            case 1:
                if (
                    graph_load(
                        &graph,
                        ROUTERS_FILE
                    )
                ) {
                    source_selected = false;
                    last_result.available = false;
                }

                break;

            case 2: {
                if (!graph.loaded) {
                    printf(
                        "Load %s first.\n",
                        ROUTERS_FILE
                    );

                    break;
                }

                char candidate;

                if (
                    !read_router(
                        "Enter the source router: ",
                        &candidate
                    )
                ) {
                    return EXIT_FAILURE;
                }

                if (
                    graph_find_router(
                        &graph,
                        candidate
                    ) == -1
                ) {
                    printf(
                        "Router '%c' does not exist "
                        "in the loaded graph.\n",
                        candidate
                    );
                } else {
                    source_router = candidate;
                    source_selected = true;
                    last_result.available = false;

                    printf(
                        "Source router set to %c.\n",
                        source_router
                    );
                }

                break;
            }

            case 3: {
                if (!graph.loaded) {
                    printf(
                        "Load %s first.\n",
                        ROUTERS_FILE
                    );

                    break;
                }

                if (!source_selected) {
                    printf(
                        "Select a source router first.\n"
                    );

                    break;
                }

                char destination_router;

                if (
                    !read_router(
                        "Enter the destination router: ",
                        &destination_router
                    )
                ) {
                    return EXIT_FAILURE;
                }

                calculate_routes(
                    &graph,
                    source_router,
                    destination_router,
                    &last_result
                );

                break;
            }

            case 4:
                save_last_result(
                    &graph,
                    &last_result,
                    OUTPUT_FILE
                );

                printf("Program terminated.\n");

                return EXIT_SUCCESS;

            default:
                printf(
                    "Choose a number from 1 to 4.\n"
                );
        }
    }

    return EXIT_SUCCESS;
}