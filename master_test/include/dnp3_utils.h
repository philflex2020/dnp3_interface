#ifndef DNP3_UTILS_H
#define DNP3_UTILS_H
/*
 * mapping shared library
 *      pwilshire      5/5/2020
 */
#include <map>

struct char_cmp {
    bool operator () (const char *a,const char *b) const
    {
        return strcmp(a,b)<0;
    }
};
typedef struct maps_t maps;
typedef std::map<const char*, std::pair<bool*, maps**>, char_cmp> body_map;
typedef std::map<const char*, body_map*, char_cmp> uri_map;

//typedef struct _modbus modbus_t;

typedef struct _dnp3_mapping_t {
    int nb_bits;
    int start_bits;
    int nb_input_bits;
    int start_input_bits;
    int nb_input_registers;
    int start_input_registers;
    int nb_registers;
    int start_registers;
    uint8_t *tab_bits;
    uint8_t *tab_input_bits;
    uint16_t *tab_input_registers;
    uint16_t *tab_registers;
} dnp3_mapping_t;

enum Type_of_Register {
    Coil, Discrete_Input, Input_Register, Holding_Register, AnalogIP, AnalogOP, BinaryIP, BinaryOP, Num_Register_Types
};

typedef struct {
    char* name;
    char* ip_address;
    char* serial_device;
    char* eth_dev;
    bool byte_swap;
    bool off_by_one;
    char parity;
    int frequency;
    int port;
    int baud;
    int data;
    int stop;
    int device_id;
    int reg_cnt;
    //modbus_t *mb;
    void *mb;
} system_config;

// one of these for each register ??
typedef struct maps_t{
    unsigned int    reg_off;
    unsigned int    num_regs;
    unsigned int    num_strings;
    int    shift;
    double scale;
    char*  reg_name;
    char*  uri;
    bool   sign;
    bool   floating_pt;
    bool   is_bool;
    bool   bit_field;
    bool   individual_bits;
    uint64_t data; // used only for individual_bits, read-modify-write construct
    bool   enum_type;
    bool   random_enum_type;
    char ** bit_strings;
    std::map<int,char*> random_enum;
    maps_t()
    {
        reg_name = uri = NULL;
        bit_strings = NULL;
        reg_off = num_regs = num_strings = 0;
        scale = 0.0;
        shift = 0;
        sign = floating_pt = is_bool = false;
        bit_field = individual_bits = enum_type = random_enum_type = false;
    }
    ~maps_t()
    {
        if(reg_name != NULL)
            free (reg_name);
        if(uri != NULL)
            free (uri);
        if(bit_strings != NULL)
        {
            for(unsigned int i = 0; i < num_regs * 16; i++)
                if(bit_strings[i] != NULL) free(bit_strings[i]);
            delete [] bit_strings;
        }
    }
} maps;

//Holds all the information related to registers whose value needs to be read
typedef struct {
    Type_of_Register reg_type;
    unsigned int start_offset;
    unsigned int num_regs;
    unsigned int map_size;
    maps *register_map;
} datalog;

// an inividual mapping register
typedef struct smapr {
    bool bvalue;
    float avalue;
    usigned int ivalue
    int type;
    smapr(int _type) {
        avalue = 0.0;
        bvalue = false;
        ivalue = 0;
        type = _type;
    }
    ~smapr(){};
    
} mapr;
// the server data type is given to the dnp3 process
// the dnp3 will load  data items by value type and index and then trigger a send
typedef struct sdata
{
    uri_map uri_to_register;
    maps** regs_to_map[Num_Register_Types];
    dnp3_mapping_t* mb_mapping;
    //void* mb_mapping;
    fims* p_fims;
    const char** uris;
    const char* base_uri;
    int num_uris;
    sdata()
    {
        memset(regs_to_map, 0, Num_Register_Types * sizeof(maps**));
        mb_mapping = NULL;
        p_fims = NULL;
        uris = NULL;
        base_uri = NULL;
        num_uris = 0;
    }
    int setValue(int idx, float value, int send) 
    {
     //   maps* mp = regs_to_map[AnalogIP];
     //   if (idx < mp->num_regs)
     //   {
     //       mapr* mr = mp->regs[idx];
     //       mapr->avalue = value;
     //       if(send)datasendAdd(mr);
     //   }
        return 0
    }

    int setValue(int idx, bool value, int send) 
    {
     //   maps* mp = regs_to_map[BinaryIP];
     //   if (idx < mp->num_regs)
     //   {
     //       mapr* mr = mp->regs[idx];
     //       mapr->bvalue = value;
     //       if(send)datasendAdd(mr);
     //   }
        return 0;
    }
    int datasendAdd( mapr * mr)
    {
        return 0;
    }

} server_data;

bool process_dnp3_message(int bytes_read, int header_length, datalog* data, system_config* config, server_data* server_map, bool serial, uint8_t * query);
bool update_register_value(dnp3_mapping_t* map, bool* reg_type, maps** settings, cJSON* value);
void emit_event(fims* pFims, const char* source, const char* message, int severity);
cJSON* get_config_json(int argc, char* argv[]);
server_data* create_register_map(cJSON* registers, datalog* data);
int parse_system(cJSON *system, system_config *config);

#endif
