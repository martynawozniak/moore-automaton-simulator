#include "ma.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

struct connection_info {
    moore_t *conn_autom;    // Automaton on the other end of connection.
    size_t n_bit;           // Position of the bit to connect.
};
typedef struct connection_info conn_info_t;

struct input_info {
    conn_info_t conn_bit;   // Source information.
    size_t conn_index;      // Index in output's connection array.
};
typedef struct input_info in_info_t;

struct output_info {
    conn_info_t *conn_bits; // Destiny information.
    size_t max_size;        // Allocated size.
    size_t curr_size;       // Part of an array in actual use.
};
typedef struct output_info out_info_t;

struct moore {
    size_t n_input;
    size_t n_output;
    size_t n_state;
    uint64_t *state;
    uint64_t *new_state;
    uint64_t *input;
    uint64_t *output;
    transition_function_t t;
    output_function_t y;
    in_info_t *src_info;      // Input source information per bit.
    out_info_t *dest_info;    // Output targets per bit.
};

// Identity output function.
static void id(uint64_t *output, uint64_t const *state, size_t, size_t s) {
    size_t size_output = (s + 63) / 64;
    memcpy(output, state, size_output * sizeof(uint64_t));
}

// Creating connection between two bits.
static int connect_bits(moore_t *a_in, moore_t *a_out, size_t in_bit,
                        size_t out_bit) {
    // Array is not allocated.
    if (!(a_out->dest_info[out_bit].conn_bits)) {
        a_out->dest_info[out_bit].max_size = 2;
        a_out->dest_info[out_bit].conn_bits = malloc(
            a_out->dest_info[out_bit].max_size * sizeof(conn_info_t)
        );
        if (!(a_out->dest_info[out_bit].conn_bits)) {
            return -1;
        }
    }

    // Resizing needed.
    if (a_out->dest_info[out_bit].curr_size ==
        a_out->dest_info[out_bit].max_size) {
        conn_info_t *temp = realloc(
            a_out->dest_info[out_bit].conn_bits,
            (a_out->dest_info[out_bit].max_size * 2) * sizeof(conn_info_t)
        );
        if (!temp) {    // Realloc was unsuccessfull.
            return -1;
        }
        a_out->dest_info[out_bit].conn_bits = temp;
        a_out->dest_info[out_bit].max_size *= 2;
    }

    // Source information for input bit.
    a_in->src_info[in_bit].conn_bit.conn_autom = a_out;
    a_in->src_info[in_bit].conn_bit.n_bit = out_bit;

    // Destiny information for output bit.
    size_t pos = a_out->dest_info[out_bit].curr_size;
    a_out->dest_info[out_bit].conn_bits[pos].conn_autom = a_in;
    a_out->dest_info[out_bit].conn_bits[pos].n_bit = in_bit;
    a_in->src_info[in_bit].conn_index = pos;
    a_out->dest_info[out_bit].curr_size++;

    return 0;
}

// Disconnecting two bits.
static void disconnect_bits(moore_t *a_in, moore_t *a_out, size_t in_bit,
                            size_t out_bit) {
    // Swaping the last element with the one to delete.
    size_t last = a_out->dest_info[out_bit].curr_size;
    size_t deleted = a_in->src_info[in_bit].conn_index;
    if (last - 1 != deleted) {
        // Information from last element to the position of deleted.
        a_out->dest_info[out_bit].conn_bits[deleted].conn_autom =
        a_out->dest_info[out_bit].conn_bits[last - 1].conn_autom;
        a_out->dest_info[out_bit].conn_bits[deleted].n_bit =
        a_out->dest_info[out_bit].conn_bits[last - 1].n_bit;
        // Deleting useless copy of the last one.
        a_out->dest_info[out_bit].conn_bits[last - 1].conn_autom = NULL;
        a_out->dest_info[out_bit].conn_bits[last - 1].n_bit = 0;
        // Changing information index in the swapped element.
        conn_info_t *moved = &a_out->dest_info[out_bit].conn_bits[deleted];
        moore_t *other = moved->conn_autom;
        size_t input_bit = moved->n_bit;
        other->src_info[input_bit].conn_index = deleted;
    }
    else {
        a_out->dest_info[out_bit].conn_bits[last - 1].conn_autom = NULL;
        a_out->dest_info[out_bit].conn_bits[last - 1].n_bit = 0;
    }

    a_out->dest_info[out_bit].curr_size--;
    
    // Setting input information as empty.
    a_in->src_info[in_bit].conn_bit.conn_autom = NULL;
    a_in->src_info[in_bit].conn_bit.n_bit = 0;
    a_in->src_info[in_bit].conn_index = 0;
}

// Updating input bit values with source information.
static void update_input(moore_t *a) {
    for (size_t i = 0; i < a->n_input; i++) {
        if (a->src_info[i].conn_bit.conn_autom) {
            size_t n_block_dest = i / 64;
            size_t bit_pos_dest = i % 64;
            size_t n_block_src = a->src_info[i].conn_bit.n_bit / 64;
            size_t bit_pos_src = a->src_info[i].conn_bit.n_bit % 64;

            uint64_t target_bit =
            (a->src_info[i].conn_bit.conn_autom->output[n_block_src] 
                >> bit_pos_src) & 1ULL;
            uint64_t src_bit = (a->input[n_block_dest] >> bit_pos_dest) & 1ULL;

            if (target_bit != src_bit) {
                a->input[n_block_dest] ^= (1ULL << bit_pos_dest);
            }
        }
    }
}

moore_t * ma_create_full(size_t n, size_t m, size_t s, transition_function_t t,
                         output_function_t y, uint64_t const *q) {
    if (m == 0 || s == 0 || !t || !y || !q) {
        errno = EINVAL;  
        return NULL;
    }

    moore_t *a = malloc(sizeof(moore_t));
    if (!a) {
        errno = ENOMEM;
        return NULL;
    }

    a->n_input = n;
    a->n_output = m;
    a->n_state = s;
    a->t = t;
    a->y = y;
    
    // Alocation of all needed memory.
    // Number of 64-bit blocks in state array.
    size_t size_state = (s + 63) / 64;
    a->state = malloc(size_state * sizeof(uint64_t));
    size_t size_input = (n + 63) / 64;
    a->input = calloc(size_input, sizeof(uint64_t));
    size_t size_output = (m + 63) / 64;
    a->output = calloc(size_output, sizeof(uint64_t));
    a->src_info = malloc(n * sizeof(in_info_t));
    a->dest_info = malloc(m * sizeof(out_info_t));
    a->new_state = calloc(size_state, sizeof(uint64_t));

    // Memory errors handling.
    if (!(a->input) || !(a->output) || !(a->state) || !(a->src_info) ||
        !(a->dest_info) || !(a->new_state)) {
        errno = ENOMEM;
        if (a->state) {
            free(a->state);
        }
        if (a->input) {
            free(a->input);
        }
        if (a->output) {
            free(a->output);
        }
        if (a->src_info) {
            free(a->src_info);
        }
        if (a->dest_info) {
            free(a->dest_info);
        }
        if (a->new_state) {
            free(a->new_state);
        }
        free(a); 
        return NULL;
    }

    // Initialization on correctly allocated elements.
    memcpy(a->state, q, size_state * sizeof(uint64_t));
    a->y(a->output, a->state, a->n_output, a->n_state);
    for (size_t i = 0; i < n; i++) {
        a->src_info[i].conn_bit.conn_autom = NULL;
        a->src_info[i].conn_bit.n_bit = 0;
        a->src_info[i].conn_index = 0;
    }
    for (size_t i = 0; i < m; i++) {
        a->dest_info[i].conn_bits = NULL;
        a->dest_info[i].max_size = 0;
        a->dest_info[i].curr_size = 0;
    }
    
    return a;
}

moore_t * ma_create_simple(size_t n, size_t s, transition_function_t t) {
    if (s == 0 || !t) {
        errno = EINVAL;
        return NULL;
    }

    // Zero-initialized initial state.
    size_t size_state = (s + 63) / 64;
    uint64_t *state = calloc(size_state, sizeof(uint64_t));
    if (state == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    // Creating automaton using existing function.
    moore_t *a_s = ma_create_full(n, s, s, t, id, state);
    free(state);    // Extra copy no longer needed.

    return a_s;
}

void ma_delete(moore_t *a) {
    if (!a) {
        return;
    }

    // Disconnecting all current connections of the deleted automaton.
    ma_disconnect(a, 0, a->n_input);
    for (size_t i = 0; i < a->n_output; i++) {
        size_t j_count = a->dest_info[i].curr_size;
        for (size_t j = 0; j < j_count; j++) {
            ma_disconnect(a->dest_info[i].conn_bits[0].conn_autom, 
                          a->dest_info[i].conn_bits[0].n_bit, 1);
        }
    }

    free(a->input);
    free(a->state);
    free(a->new_state);
    free(a->output);
    free(a->src_info);
    for (size_t i = 0; i < a->n_output; i++) {
        free(a->dest_info[i].conn_bits);
    }
    free(a->dest_info);
    free(a);
}

int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out,
               size_t num) {
    int result = 0;
    if (!a_in || !a_out || num == 0 || in + num > a_in->n_input ||
        out + num > a_out->n_output) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < num; i++) {
        // Previous connection exists.
        if (a_in->src_info[in + i].conn_bit.conn_autom != NULL) {
            disconnect_bits(a_in, a_in->src_info[in + i].conn_bit.conn_autom,
                            in + i, a_in->src_info[in + i].conn_bit.n_bit);
        }
        result = connect_bits(a_in, a_out, in + i, out + i);
        if (result == -1) {
            errno = ENOMEM;
            return -1;
        }
    }

    return 0;
}

int ma_disconnect(moore_t *a_in, size_t in, size_t num) {
    if (!a_in || num == 0 || in + num > a_in->n_input) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        // Previous connection exists.
        if (a_in->src_info[in + i].conn_bit.conn_autom != NULL) {
            disconnect_bits(a_in, a_in->src_info[in + i].conn_bit.conn_autom,
                            in + i, a_in->src_info[in + i].conn_bit.n_bit);
        }
    }

    return 0;
}

int ma_set_input(moore_t *a, uint64_t const *input) {
    if (!a || !input || a->n_input == 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < a->n_input; i++) {
        // Bit is not connected.
        if (a->src_info[i].conn_bit.conn_autom == NULL) {
            size_t n_block = i / 64;
            size_t bit_pos = i % 64;

            uint64_t target_bit = (input[n_block] >> bit_pos) & 1ULL;
            uint64_t src_bit = (a->input[n_block] >> bit_pos) & 1ULL;

            if (target_bit != src_bit) {
                // Flip the bit only if value not desired.
                a->input[n_block] ^= (1ULL << bit_pos);
            }
        }
    }

    return 0;
}

int ma_set_state(moore_t *a, uint64_t const *state) {
    if (!a || !state) {
        errno = EINVAL;
        return -1;
    }

    size_t n_blocks = (a->n_state + 63) / 64;
    for (size_t i = 0; i < n_blocks; i++) {
        a->state[i] = state[i];
    }

    // Updating output.
    a->y(a->output, a->state, a->n_output, a->n_state);
    
    return 0;
}

uint64_t const * ma_get_output(moore_t const *a) {
    if (!a) {
        errno = EINVAL;
        return NULL;
    }

    return a->output;
}

int ma_step(moore_t *at[], size_t num) {
    if (!at || num == 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        if (!at[i]) {
            errno = EINVAL;
            return -1;
        }
    }

    // Ensuring inputs are based on the outputs from before the function call.
    for (size_t i = 0; i < num; i++) {
        update_input(at[i]);
    }

    // Calculating new states and outputs.
    for (size_t i = 0; i < num; i++) {
        at[i]->t(at[i]->new_state, at[i]->input, at[i]->state, at[i]->n_input,
                 at[i]->n_state);
        size_t size_state = (at[i]->n_state + 63) / 64;
        memcpy(at[i]->state, at[i]->new_state, size_state * sizeof(uint64_t));
        at[i]->y(at[i]->output, at[i]->state, at[i]->n_output, at[i]->n_state);
    }

    return 0;
}