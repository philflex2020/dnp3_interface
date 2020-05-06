/* this is the outline of a master dnp3 controller
* the system can communicate with a number outstations
* The confg file will determine the address info on each outstation.
* each outsation will have a number of data points.
* a feature of dnp3 is the need to povide a shadow  stack to hold the user defined data points
* iniitally a copy of the modbus stuff
*/
/*
 * dnp3_master.cpp
 *
 *  Created on: Sep 28, 2020
 *      Author: jcalcagni - modbus
 *              pwilshire - dnp3 version
 */

#include <stdio.h>
#include <string>
#include <string.h>
#include <map>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <signal.h>

#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <fims/libfims.h>

#include <iostream>
#include <asiopal/UTCTimeSource.h> 
#include <opendnp3/LogLevels.h> 
#include <asiodnp3/ConsoleLogger.h> 
#include <asiodnp3/DNP3Manager.h> 
#include <asiodnp3/DefaultMasterApplication.h> 
#include <asiodnp3/PrintingChannelListener.h> 
#include <asiodnp3/PrintingCommandCallback.h> 
#include <asiopal/ChannelRetry.h>

#include "newSOEHandler.h"
//#include <modbus/modbus.h>
#include "dnp3_utils.h"
using namespace std; 
using namespace openpal; 
using namespace asiopal; 
using namespace asiodnp3; 
using namespace opendnp3; 
#define MICROSECOND_TO_MILLISECOND 1000
#define NANOSECOND_TO_MILLISECOND  1000000

volatile bool running = true;

void signal_handler (int sig)
{
    running = false;
    fprintf(stderr, "signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}

int establish_connection(system_config* config)
{
    struct ifreq ifr;
    struct sockaddr_in addr;
    int sock, yes;
    // modbus listen only accepts on INADDR_ANY so need to make own socket and hand it over
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
    {
        fprintf(stderr, "Failed to create socket: %s.\n", strerror(errno));
        return -1;
    }

    yes = 1;
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes)))
    {
        fprintf(stderr, "Failed to set socket option reuse address: %s.\n", strerror(errno));
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    //read port out of config
    addr.sin_port = htons(config->port);

    if(config->eth_dev != NULL)
    {
        ifr.ifr_addr.sa_family = AF_INET;
        //read interface out of config file
        strncpy(ifr.ifr_name, config->eth_dev, IFNAMSIZ - 1);
        if(-1 == ioctl(sock, SIOCGIFADDR, &ifr))
        {
            fprintf(stderr, "Failed to get ip address for interface %s: %s.\n", ifr.ifr_name, strerror(errno));
            close(sock);
            return -1;
        }
        addr.sin_addr.s_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    }
    else if(config->ip_address != NULL)
        addr.sin_addr.s_addr = inet_addr(config->ip_address);
    else
        addr.sin_addr.s_addr = INADDR_ANY;

    if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        fprintf(stderr, "Failed to bind socket: %s.\nIf trying to use a port less than 1024 run programs as sudo.\n", strerror(errno));
        close(sock);
        return -1;
    }

    if (-1 == listen(sock, 1))
    {
        fprintf(stderr, "Failed to listen on socket: %s.\n", strerror(errno));
        close(sock);
        return -1;
    }

    // Values passed to modbus_new_tcp are ignored since we are listening on our own socket
    //config->mb = modbus_new_tcp("127.0.0.1", MODBUS_TCP_DEFAULT_PORT);
    //if(config->mb == NULL)
    //{
    //    fprintf(stderr, "Failed to allocate context.\n");
    //    close(sock);
    //    return -1;
    //}
    //if(config->device_id != -1 && -1 == modbus_set_slave(config->mb, config->device_id))
    //{
    //    fprintf(stderr, "Valid Unit identifiers (Device_Id) are between 1-247.\n");
    //    modbus_free(config->mb);
    //    config->mb = NULL;
    //    return -1;
    //}
    return sock;
}


bool process_fims_message(fims_message* msg, server_data* server_map)
{

    char* uri = msg->uri;
    // uri was sent to this processes URI so it needs to handle it
    if(strncmp(uri, server_map->base_uri, strlen(server_map->base_uri)) == 0)
    {
        if(strcmp("set", msg->method) == 0)
        {
            uri = msg->pfrags[2];
            if(msg->nfrags > 2 && strncmp("reply", uri, strlen("reply")) == 0)
            {
                uri = msg->pfrags[3] - 1;
                uri_map::iterator uri_it = server_map->uri_to_register.find(uri);
                if(uri_it == server_map->uri_to_register.end())
                {
                    // failed to find in map
                    fprintf(stderr, "Fims reply for unrequested uri: %s.\n", uri);
                    return false;
                }
                body_map* body = uri_it->second;
                cJSON* body_obj = cJSON_Parse(msg->body);
                //check for valid json
                if(body_obj == NULL)
                {
                    fprintf(stderr, "Received invalid json object for set %s. \"%s\"\n", uri, msg->body);
                    return false;
                }
                for(body_map::iterator body_it = body->begin(); body_it != body->end(); ++body_it)
                {
                    cJSON* cur_obj = cJSON_GetObjectItem(body_obj, body_it->first);
                    if(cur_obj == NULL)
                    {
                        fprintf(stderr, "Failed to find %s in %s.\n", body_it->first, uri);
                        return false;
                    }
                    update_register_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, cur_obj);
                }
            }
            else
            {
                if(msg->replyto != NULL)
                    server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
                return true;
            }
        }
        else if(strcmp("get", msg->method) == 0)
        {
            //don't know when we would receive a get
            if(msg->replyto != NULL)
                server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
            return true;
        }
        else
        {
            if(msg->replyto != NULL)
                server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
            return true;
        }
    }
    else // Check to see if this is a publish we care about
    {
        if(strcmp("pub", msg->method) == 0)
        {
            uri_map::iterator uri_it = server_map->uri_to_register.find(msg->uri);
            if(uri_it == server_map->uri_to_register.end())
            {
                //failed to find uri in devices we care about.
                fprintf(stderr, "Received pub not in uri list: %s.\n", msg->uri);
                return false;
            }
            cJSON* body_obj = cJSON_Parse(msg->body);
            if(body_obj == NULL || body_obj->child == NULL)
            {
                if(body_obj != NULL)
                    cJSON_Delete(body_obj);
                return false;
            }
            for(cJSON* id_obj = body_obj->child; id_obj != NULL; id_obj = id_obj->next)
            {
                body_map::iterator body_it = uri_it->second->find(id_obj->string);
                if(body_it == uri_it->second->end())
                {
                    // Value not in our array ignore
                    continue;
                }
                update_register_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, id_obj);
            }
            cJSON_Delete(body_obj);
        }
        else
            // Not our message so ignore
            return true;
    }
    return true;
}

bool initialize_map(server_data* server_map)
{
    if(server_map->p_fims->Subscribe((const char**)server_map->uris, server_map->num_uris + 1) == false)
    {
        fprintf(stderr, "Failed to subscribe to URI.\n");
        return false;
    }

    char replyto[256];
    for(uri_map::iterator it = server_map->uri_to_register.begin(); it != server_map->uri_to_register.end(); ++it)
    {
        sprintf(replyto, "%s/reply%s", server_map->base_uri, it->first);
        server_map->p_fims->Send("get", it->first, replyto, NULL);
        timespec current_time, timeout;
        clock_gettime(CLOCK_MONOTONIC, &timeout);
        timeout.tv_sec += 2;
        bool found = false;

        while(server_map->p_fims->Connected() && found != true &&
             (clock_gettime(CLOCK_MONOTONIC, &current_time) == 0) && (timeout.tv_sec > current_time.tv_sec ||
             (timeout.tv_sec == current_time.tv_sec && timeout.tv_nsec > current_time.tv_nsec + 1000)))
        {
            unsigned int us_timeout_left = (timeout.tv_sec - current_time.tv_sec) * 1000000 +
                    (timeout.tv_nsec - current_time.tv_nsec) / 1000;
            fims_message* msg = server_map->p_fims->Receive_Timeout(us_timeout_left);
            if(msg == NULL)
                continue;
            bool rc = process_fims_message(msg, server_map);
            if(strcmp(replyto, msg->uri) == 0)
            {
                found = true;
                if(rc == false)
                {
                    fprintf(stderr, "Error, failed to find value from config file in defined uri.\n");
                    server_map->p_fims->free_message(msg);
                    return false;
                }
            }
            server_map->p_fims->free_message(msg);
        }
        if(found == false)
        {
            fprintf(stderr, "Failed to find get response for %s.\n", it->first);
            return false;
        }
    }
    return true;
}
#if 1
void setupDNP3(void)
{
     // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_APP_COMMS;
    // This is the main point of interaction with the stack
    DNP3Manager manager(1, ConsoleLogger::Create());
    // Connect via a TCPClient socket to a outstation
    // repeat for each outstation
    auto channel = manager.AddTCPClient("tcpclient", FILTERS, ChannelRetry::Default(), {IPEndpoint("127.0.0.1", 20001)},
                                        "0.0.0.0", PrintingChannelListener::Create());
    // The master config object for a master. The default are
    // useable, but understanding the options are important.
    MasterStackConfig stackConfig;
    // you can override application layer settings for the master here
    // in this example, we've change the application layer timeout to 2 seconds
    stackConfig.master.responseTimeout = TimeDuration::Seconds(2);
    stackConfig.master.disableUnsolOnStartup = true;
    // You can override the default link layer settings here
    // in this example we've changed the default link layer addressing
    stackConfig.link.LocalAddr = 1;
    stackConfig.link.RemoteAddr = 10;
    // Create a new master on a previously declared port, with a
    // name, log level, command acceptor, and config info. This
    // returns a thread-safe interface used for sending commands.
    void * ourDB = NULL;
    auto newSOE = newSOEHandler::Create(ourDB);
    //newSOE.setDB(ourDB);
    auto master = channel->AddMaster("master", // id for logging
                                     newSOE, // callback for data processing
                                     asiodnp3::DefaultMasterApplication::Create(), // master application instance
                                     stackConfig // stack configuration
    );
    // do an integrity poll (Class 3/2/1/0) once per minute
    auto integrityScan = master->AddClassScan(ClassField::AllClasses(), TimeDuration::Minutes(1));
    // do a Class 1 exception poll every 5 seconds
    auto exceptionScan = master->AddClassScan(ClassField(ClassField::CLASS_1), TimeDuration::Seconds(20));
    auto objscan = master->AddAllObjectsScan(GroupVariationID(30,1),
                                                                   TimeDuration::Seconds(10));
    // Enable the master. This will start communications.
    master->Enable();
    //bool channelCommsLoggingEnabled = true;
    //bool masterCommsLoggingEnabled = true;
}
#endif

int main(int argc, char *argv[])
{
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    server_data *server_map = NULL;
    //int header_length,;
    //int serial_fd;
    int fims_socket;
    int fd_max = 0;
    int rc = 0;
    //int server_socket = -1;
    int fims_connect = 0;
    fd_set all_connections;
    FD_ZERO(&all_connections);
    system_config sys_cfg;
    memset(&sys_cfg, 0, sizeof(system_config));
    datalog data[Num_Register_Types];
    memset(data, 0, sizeof(datalog) * Num_Register_Types);

    fprintf(stderr, "Reading config file and starting setup.\n");
    cJSON* config = get_config_json(argc, argv);
    if(config == NULL)
        return 1;

    // Getting the object "System Name" from the object  "System' //
    cJSON *system_obj = cJSON_GetObjectItem(config, "system");
    if (system_obj == NULL)
    {
        fprintf(stderr, "Failed to get System object in config file.\n");
        cJSON_Delete(config);
        return 1;
    }

    cJSON *registers = cJSON_GetObjectItem(config, "registers");
    if (registers == NULL)
    {
        fprintf(stderr, "Failed to get object item 'register' in JSON.\n");
        cJSON_Delete(config);
        return 1;
    }

    // Read connection information from json and establish modbus connection
    if(-1 == parse_system(system_obj, &sys_cfg))
    {
        cJSON_Delete(config);
        return 1;
    }
    fprintf(stderr, "System config complete: Setup register map.\n");
    server_map = create_register_map(registers, data);
    cJSON_Delete(config);
    if (server_map == NULL)
    {
        fprintf(stderr, "Failed to initialize the mapping\n");
        rc = 1;
        goto cleanup;
    }

    server_map->p_fims = new fims();
    if (server_map->p_fims == NULL)
    {
        fprintf(stderr,"Failed to allocate connection to FIMS server.\n");
        rc = 1;
        goto cleanup;
    }
    // could alternatively fims connect using a stored name for the server
    while(fims_connect < 5 && server_map->p_fims->Connect(sys_cfg.name) == false)
    {
        fims_connect++;
        sleep(1);
    }
    if(fims_connect >= 5)
    {
        fprintf(stderr,"Failed to establish connection to FIMS server.\n");
        rc = 1;
        goto cleanup;
    }

    fprintf(stderr, "Map configured: Initializing data.\n");
    //todo this should be defined to a better length
    char uri[100];
    sprintf(uri,"/interfaces/%s", sys_cfg.name);
    server_map->base_uri = server_map->uris[0] = uri;
    if(false == initialize_map(server_map))
    {
        fprintf(stderr, "Failed to get data for defined URI.\n");
        rc = 1;
        goto cleanup;
    }
    fprintf(stderr, "Data initialized: setting up dnp3 connection.\n");
#if 0
    if(sys_cfg.serial_device == NULL)
    {
        server_socket = establish_connection(&sys_cfg);
        if(server_socket == -1)
        {
            char message[1024];
            snprintf(message, 1024, "Modbus Server %s failed to establish open TCP port to listen for incoming connections.\n", sys_cfg.name);
            fprintf(stderr, "%s\n", message);
            emit_event(server_map->p_fims, "Modbus Server", message, 1);
            rc = 1;
            goto cleanup;
        }
        char message[1024];
        snprintf(message, 1024, "Modbus Server %s, listening for connections on %s port %d.\n", sys_cfg.name, sys_cfg.ip_address, sys_cfg.port);
        fprintf(stderr, "%s\n", message);
        emit_event(server_map->p_fims, "Modbus Server", message, 1);
                    
    }
    else
    {
        //read from configure
        sys_cfg.mb = modbus_new_rtu(sys_cfg.serial_device, sys_cfg.baud, sys_cfg.parity, sys_cfg.data, sys_cfg.stop);
        if(sys_cfg.mb == NULL)
        {
            fprintf(stderr, "Failed to allocate new modbus context.\n");
            rc = 1;
            goto cleanup;
        }
        // set device id from config
        if(-1 == modbus_set_slave(sys_cfg.mb, sys_cfg.device_id))
        {
            fprintf(stderr, "Serial devices need a valid Device_Id between 1-247.\n");
            rc = 1;
            goto cleanup;
        }
        if(-1 == modbus_connect(sys_cfg.mb))
        {
            char message[1024];
            snprintf(message, 1024, "Modbus Server %s failed to connect on interface %s: %s.\n", sys_cfg.name, sys_cfg.serial_device, modbus_strerror(errno));
            fprintf(stderr, "%s\n", message);
            emit_event(server_map->p_fims, "Modbus Server", message, 1);
            rc = 1;
            goto cleanup;
        }
        char message[1024];
        snprintf(message, 1024, "Modbus Server %s, serial connection established on interface %s.\n", sys_cfg.name, sys_cfg.serial_device);
        fprintf(stderr, "%s\n", message);
        emit_event(server_map->p_fims, "Modbus Server", message, 1);
                    
    }
#endif

    fd_set connections_with_data;
    fims_socket = server_map->p_fims->get_socket();
    //serial_fd = -1;
    //header_length = modbus_get_header_length(sys_cfg.mb);

    //if(server_socket != -1)
    //{
    //    FD_SET(server_socket, &all_connections);
    //    fd_max = server_socket;
    //}
    //else
    //{
    //    serial_fd = modbus_get_socket(sys_cfg.mb);
    //    if(serial_fd == -1)
    //    {
    //        fprintf(stderr, "Failed to get serial file descriptor.\n");
    //        rc = 1;
    //        goto cleanup;
    //    }
    //    FD_SET(serial_fd, &all_connections);
    //    fd_max = serial_fd;
    //}

    if(fims_socket != -1)
        FD_SET(fims_socket, &all_connections);
    else
    {
        fprintf(stderr, "Failed to get fims socket.\n");
        rc = 1;
        goto cleanup;
    }

    fd_max = (fd_max > fims_socket) ? fd_max: fims_socket;

    fprintf(stderr, "Setup complete: Entering main loop.\n");
    while(running)
    {
        connections_with_data = all_connections;
        // Select will block until one of the file descriptors has data
        if(-1 == select(fd_max+1, &connections_with_data, NULL, NULL, NULL))
        {
            fprintf(stderr, "server select() failure: %s.\n", strerror(errno));
            break;
        }
        //Loop through file descriptors to see which have data to read
        for(int current_fd = 0; current_fd <= fd_max; current_fd++)
        {
            // if no data on this file descriptor skip to the next one
            if(!FD_ISSET(current_fd, &connections_with_data))
                continue;
#if 0
            if(current_fd == server_socket)
            {
                // A new client is connecting to us
                socklen_t address_length;
                struct sockaddr_in client_address;
                int new_fd;

                address_length = sizeof(client_address);
                memset(&client_address, 0, sizeof(address_length));
                new_fd = accept(server_socket, (struct sockaddr *)&client_address, &address_length);
                if(new_fd == -1)
                {
                    fprintf(stderr, "Error accepting new connections: %s\n", strerror(errno));
                    break;
                }
                else
                {
                    char message[1024];
                    FD_SET(new_fd, &all_connections);
                    fd_max = (new_fd > fd_max) ? new_fd : fd_max;
                    snprintf(message, 1024, "New connection from %s:%d on interface %s",
                       inet_ntoa(client_address.sin_addr), client_address.sin_port, sys_cfg.name);
                    fprintf(stderr, "%s\n", message);
                    emit_event(server_map->p_fims, "Modbus Server", message, 1);
                }
            }
            else
#endif
            if(current_fd == fims_socket)
            {
                // Fims message received
                fims_message* msg = server_map->p_fims->Receive_Timeout(1*MICROSECOND_TO_MILLISECOND);
                if(msg == NULL)
                {
                    if(server_map->p_fims->Connected() == false)
                    {
                        // fims connection closed
                        fprintf(stderr, "Fims connection closed.\n");
                        FD_CLR(current_fd, &all_connections);
                        break;
                    }
                    else
                        fprintf(stderr, "No fims message. Select led us to a bad place.\n");
                }
                else
                {
                    process_fims_message(msg, server_map);
                    server_map->p_fims->free_message(msg);
                }
            }
#if 0
            else if(current_fd == serial_fd)
            {
                // incoming serial modbus communication
                uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

                rc = modbus_receive(sys_cfg.mb, query);
                if (rc > 0)
                {
                    process_modbus_message(rc, header_length, data, &sys_cfg, server_map, true, query);
                    modbus_reply(sys_cfg.mb, query, rc, server_map->mb_mapping);
                }
                else if (rc  == -1)
                {
                    // Connection closed by the client or error, log error
                    char message[1024];
                    snprintf(message, 1024, "Modbus Server %s, communication error. Closing serial connection to recover. Error: %s\n", sys_cfg.name, modbus_strerror(errno));
                    fprintf(stderr, "%s\n", message);
                    emit_event(server_map->p_fims, "Modbus Server", message, 1);
                    modbus_close(sys_cfg.mb);
                    // likely redundant close
                    close(serial_fd);
                    FD_CLR(current_fd, &all_connections);
                    if(current_fd == fd_max)
                        fd_max--;
                    running = false;
                    break;
                }
            }
            else
            {
                // incoming tcp modbus communication
                modbus_set_socket(sys_cfg.mb, current_fd);
                uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

                rc = modbus_receive(sys_cfg.mb, query);
                if (rc > 0)
                {
                    process_modbus_message(rc, header_length, data, &sys_cfg, server_map, false, query);
                    modbus_reply(sys_cfg.mb, query, rc, server_map->mb_mapping);
                }
                else if (rc  == -1)
                {
                    // Connection closed by the client or error
                    char message[1024];
                    snprintf(message, 1024, "Modbus Server %s detected a TCP client disconnect.\n", sys_cfg.name);
                    fprintf(stderr, "%s\n", message);
                    emit_event(server_map->p_fims, "Modbus Server", message, 1);
                    close(current_fd);
                    FD_CLR(current_fd, &all_connections);
                    if(current_fd == fd_max)
                        fd_max--;
                }
            }
#endif
        }
    }

    fprintf(stderr, "Main loop complete: Entering clean up.\n");

    cleanup:

    if(sys_cfg.eth_dev       != NULL) free(sys_cfg.eth_dev);
    if(sys_cfg.ip_address    != NULL) free(sys_cfg.ip_address);
    if(sys_cfg.name          != NULL) free(sys_cfg.name);
    if(sys_cfg.serial_device != NULL) free(sys_cfg.serial_device);
    //if(sys_cfg.mb != NULL)             modbus_free(sys_cfg.mb);
    if(server_map != NULL)
    {
        //for (uri_map::iterator it = server_map->uri_to_register.begin(); it != server_map->uri_to_register.end(); ++it)
        //{
         //   for (body_map::iterator body_it = it->second->begin(); body_it != it->second->end(); ++body_it)
         //   {
         //       delete []body_it->second.first;
         //       delete []body_it->second.second;
          //  }
         //   it->second->clear();
         //   delete(it->second);
        //}
        //server_map->uri_to_register.clear();
        for (int i = 0; i < Num_Register_Types; i++)
        {
            if(data[i].register_map != NULL)
                delete []data[i].register_map;
            if(server_map->regs_to_map[i] != NULL) delete [] server_map->regs_to_map[i];
        }
        if (server_map->uris != NULL) delete []server_map->uris;
        if(server_map->p_fims != NULL)
        {
            if(server_map->p_fims->Connected() == true)
            {
                FD_CLR(server_map->p_fims->get_socket(), &all_connections);
                server_map->p_fims->Close();
            }
            delete server_map->p_fims;
        }
//        if(server_map->mb_mapping != NULL)
//            modbus_mapping_free(server_map->mb_mapping);
        delete server_map;
    }
    for(int fd = 0; fd < fd_max; fd++)
        if(FD_ISSET(fd, &all_connections))
            close(fd);
    return rc;
}

