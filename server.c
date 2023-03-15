#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

// int setupserver
// listen for connection
// give a message on accept
// get the reply on demand
// decide whether the client
// is student or TA
// show list of questions
// choose one
// setup a room
// broadcast

#define DEFAULT_SERVER_PORT 8090
#define MAX_CLIENT 50
#define SERVER_BROADCAST "127.255.255.255"
#define MAX_QUESTION 100
#define BUFFER_SIZE 1024
#define QUESTION_LENGTH 256
#define MAX_ROOM 20
#define SERVER_UP "Server UP!\n"
#define NEW_CLIENT "New Client!\n"
#define CHOOSE_ROLE "Please Enter Your Role\n1)TA\n2)Student\n"
#define CLIENT_CLOSED "Client Closed Connection"
#define UKNOWN_STAT "Uknown Stat Error"
#define UKNOWN_OPTION "Uknown Option Pressed"
#define TA_OPTION "1\n"
#define STUDENT_OPTION "2\n"
#define STUDENT_MENU "Press:\n1)To Ask a Question\n2)To Spectate a Session\n"
#define STUDENT_ASK_OPT "1\n"
#define STUDENT_SPECT_OPT "2\n"
#define STUDENT_WHAT_QUESTION "Please Enter Your Question:\n"
#define STUDENT_WAIT "Please Wait for a TA to Accept Your Question\n"
#define TA_MENU "Please Choose From the Menu:\n0)to Do Nothing\n1)Inspect List of Questions\n"
#define TA_NO_OPT "0\n"
#define TA_LIST_QUESTION "1\n"
#define NO_QUESTION "There is No Questions Yet Please Wait While The Students Submit Their Questions\n"
#define DELIM "~~~~~~~~~~~~~~~~~\n"
#define END_OF_QUESTIONS "~End of Questions\n"
#define TA_CHOOSE_QUESTION "Dear TA, Please Choose the Question You Want to Answer (Enter the Question Number)\n"
#define ERROR_QUESTION "Your Question Number is not Vali. Please Enter Another One.\n"    
#define TA_WAIT "Please wait for the student to choose a room\n"
#define TA_ACCEPTED "a TA has accepted your request, please choose from the rooms\n"
#define ROOM_NOT_VALID "The room you entered is not valid, please enter another one from available rooms\n"
#define NOT_VALID_ROOM "The"
#define PATTERN "$$$"
#define NOW_IN_ROOM "You are now in room:\nChat:\n" 
#define EXIT_PAT "exit\n"
#define STUDENT_ANSWER "What was the answer to your question?\n"

enum Role
{
    TA = 'T',
    STUDENT = 'S',
    WAIT = 'W'
};

enum QstStatus
{
    ANSWERED,
    IN_SESSION,
    NOT_CHOSEN
};

enum UsrStatus
{
    MENU = 1,
    ACTION_MENU = 2,
    ASKED_QUESTION = 3,
    AWAIT_RESPONSE = 4,
    IN_TALK = 5,
    WATCHING_SESSION = 6,
    QUESTION_ANSWERED = 7,
    TA_LIST = 8,
    CHOOSE_ROOM = 9
};

enum RoomStatus
{
    USED,
    UNUSED
};

struct Question
{
    char* question;
    char* answer;
    int st_fd;
    int ta_fd;
    enum QstStatus q_stat;
    int qst_id;
};

struct Client
{
    int user_fd;
    enum Role usr_type;
    enum UsrStatus usr_stat;
};

struct Room
{
    int ta_fd;
    int student_fd;
    int port;
    int qst_in_room;
    enum RoomStatus rm_stat;
    struct sockaddr_in* rm_broadcast;
    int rm_socket_fd;
};

typedef struct Question Qst;
typedef struct Client Cli;
typedef struct Room Rm;

int setup_server(int port);

int accept_client(int serverFD);

void run_server(int serverFD, Qst* qst_set, Cli* cli_set,
    Rm* room_set);

int new_client(int serverFD, Cli* cli_set);

void manage_client(int serverFD, int client_fd, 
    Qst* qst_set, Cli* cli_set, fd_set* mset, Rm* room_set);

void role_handler(int fd, Cli* cli_set, const char* buffer,
    Qst* qst_set, Rm* room_set);

void status_handler(int fd, Cli* cli_set, enum UsrStatus stat, 
    const char* buffer);

void TA_handler(int t_fd, Cli* cli_set, Qst* qst_set, 
    const char* read_buf, Rm* room_set);

void add_question_set(const char* buffer,int cli_fd,
    Qst* qst_set);

void student_handler(int s_fd, Cli* cli_set, Qst* qst_set, 
    const char* buffer, Rm* rm_set);

int did_ta_accept(int std_fd, Qst* qst_set);

int show_question_set(int client_fd, Qst* qst_set);

void assign_ta_student(Cli* cli_set, const char* buffer, int t_fd, 
    Qst* qst_set, int* ta_student);

void init_rooms(Rm* room_set);

void show_session_lists(int rec_fd, Rm* room_set);

int student_chosen_room(const char* buffer, int s_fd, Qst* qst_set,
    Rm* rm_set);

Rm* chosen_room_available(Rm* rm_set, int rm_num);

int return_taf_from_stf(Qst* q_set, int sfd);

int return_qID_from_fd(int fd, Qst* q_set);

void send_pattern(int s_fd, Qst* q_set, int port);

void setup_room_broadcast(struct sockaddr_in* sockadr, int port, 
    int sock);

void set_rooms(fd_set* master, Rm* rm_set);

Rm* return_room_from_fd(int fd, Rm* rm_set);

enum Role return_role_from_fd(int fd, Cli* cli_set);

void broadcast_message(Rm* rm_set, Qst* q_set, int fd, const char* buffer, 
    Cli* cli_set);

static int qID = 0;

int main(int argc, char* argv)
{
    Qst qst_set[MAX_QUESTION];
    Cli client_set[MAX_CLIENT];
    Rm room_set[MAX_ROOM];
    int serverFD = setup_server(DEFAULT_SERVER_PORT);
    init_rooms(room_set);
    run_server(serverFD, qst_set, client_set, room_set);
}   


int setup_server(int port)
{
    struct sockaddr_in socketAddr;
    int serverFD;
    int opt = 1;
    serverFD = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, &opt,
    (socklen_t)sizeof(opt));
    socketAddr.sin_addr.s_addr = INADDR_ANY;
    socketAddr.sin_family = AF_INET;
    socketAddr.sin_port = htons(port);
    bind(serverFD, (const struct sockaddr*)&socketAddr,
        (socklen_t) sizeof(socketAddr));
    listen(serverFD, 12);
    // +
    // char temp[100];
    // sprintf(temp, "server with fd: %d is up\n", serverFD);
    // write(1, temp, 100);
    // +
    return serverFD;
}

int accept_client(int serverFD)
{
    int client_fd;
    struct sockaddr_in client_socket;
    int cli_addr_len = sizeof(client_socket);
    client_fd = accept(serverFD,(struct sockaddr*) &client_socket,
        (socklen_t*)&cli_addr_len);
    if (client_fd < 0)
    {
        perror("Error Accepting Connection");
    }
    // -
    // char t[14];
    // sprintf(t, "c accept %d", client_fd);
    // write(1, t, strlen(t));
    // -
    return client_fd;
}

void run_server(int serverFD, Qst* qst_set, Cli* cli_set,
    Rm* room_set)
{
    char buffer[BUFFER_SIZE] = {'\0'};
    fd_set temp, master_set;
    FD_ZERO(&master_set);
    write(1, SERVER_UP,(size_t)strlen(SERVER_UP));
    int max_sd = serverFD;
    FD_SET(serverFD, &master_set);
    set_rooms(&master_set, room_set);
    while(1)
    {
        temp = master_set;
        select(max_sd+1, &temp, NULL, NULL, NULL);
        for(int i = 0; i <= max_sd; i++)
        {
            if(FD_ISSET(i, &temp))
            {
                if(i == serverFD) // New Client
                {
                    int new_socket = new_client(serverFD, cli_set);
                    FD_SET(new_socket, &master_set);
                    if(new_socket > max_sd)
                    {
                        max_sd = new_socket;
                    }
                }
                else if(i > serverFD && i < serverFD+MAX_ROOM)
                {
                    break;// DO nothing
                }
                else // Listen to Client
                {
                    manage_client(serverFD, i, qst_set, cli_set
                        , &master_set, room_set);
                }
            }
        }
    }
}

int new_client(int serverFD, Cli* cli_set)
{
    int new_socket = accept_client(serverFD);
    write(1, NEW_CLIENT, strlen(NEW_CLIENT));
    send(new_socket, CHOOSE_ROLE, strlen(CHOOSE_ROLE), 0);
    memset(&cli_set[new_socket], 0, sizeof(cli_set[new_socket]));
    cli_set[new_socket].user_fd = new_socket;
    cli_set[new_socket].usr_stat = MENU;
    cli_set[new_socket].usr_type = WAIT;
    return new_socket;
}

void manage_client(int serverFD, int client_fd, 
    Qst* qst_set, Cli* cli_set, fd_set* mset, Rm* room_set)
{
    char buffer[BUFFER_SIZE];
    int number_of_bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if(number_of_bytes == 0)
    {
        int log = open("log.txt", O_APPEND, O_CREAT);
        write(log, &client_fd, sizeof(client_fd));
        write(log, CLIENT_CLOSED, strlen(CLIENT_CLOSED));
        close(log);
        close(client_fd);
        FD_CLR(client_fd, mset);
    }
    else
    {
        role_handler(client_fd, cli_set, buffer, qst_set,
            room_set); 
    }
    memset(buffer, 0, BUFFER_SIZE);
}

void role_handler(int fd, Cli* cli_set, const char* buffer,
    Qst* qst_set, Rm* room_set)
{
    if(cli_set[fd].usr_type == WAIT)
    {
        if(strcmp(TA_OPTION, buffer) == 0) // is TA
        {
            cli_set[fd].usr_type = TA;
            cli_set[fd].usr_stat = ACTION_MENU;
            send(fd, TA_MENU, strlen(TA_MENU), 0);
        }
        else if(strcmp(STUDENT_OPTION, buffer) == 0) // is Student
        {
            cli_set[fd].usr_type = STUDENT;
            cli_set[fd].usr_stat = ACTION_MENU;
            send(fd, STUDENT_MENU, strlen(STUDENT_MENU), 0);
        }
        else
        {
            perror(UKNOWN_OPTION);
            int log = open("log.txt", O_APPEND, O_CREAT);
            write(log, UKNOWN_OPTION, strlen(UKNOWN_OPTION));
            close(log);   
        }
    }
    else if(cli_set[fd].usr_type == TA)
    {
        TA_handler(fd, cli_set, qst_set, buffer, room_set);
    }
    else if(cli_set[fd].usr_type == STUDENT)
    {
        student_handler(fd, cli_set, qst_set, buffer, room_set);
    }
    else
    {
        perror(UKNOWN_STAT);
        int log = open("log.txt", O_APPEND, O_CREAT);
        write(log, UKNOWN_STAT, strlen(UKNOWN_STAT));
        close(log);   
    }
}

void TA_handler(int t_fd, Cli* cli_set, Qst* qst_set, 
    const char* read_buf, Rm* room_set)
{
    if(cli_set[t_fd].usr_stat == ACTION_MENU)
    {
        if(strcmp(read_buf, TA_NO_OPT) == 0)
        {
            send(t_fd, TA_MENU, strlen(TA_MENU), 0);
        }
        if(strcmp(read_buf, TA_LIST_QUESTION) == 0)
        {
            if(show_question_set(t_fd, qst_set))
            {
                cli_set[t_fd].usr_stat = TA_LIST;   
            }
            return;
        }
    }
    else if(cli_set[t_fd].usr_stat == TA_LIST)
    {
        int ta_student[2];
        ta_student[0] = -5;
        assign_ta_student(cli_set, read_buf,
            t_fd, qst_set, ta_student);
        if(ta_student[0] == -5)
        {
            send(t_fd, ERROR_QUESTION, strlen(ERROR_QUESTION), 0);
            return;
        }
        else
        {
            send(ta_student[0], TA_WAIT, strlen(TA_WAIT), 0);
            // send ta accept
            show_session_lists(ta_student[1], room_set);
            send(ta_student[1], TA_ACCEPTED, strlen(TA_ACCEPTED), 0);
            cli_set[ta_student[1]].usr_stat = CHOOSE_ROOM;
            cli_set[ta_student[0]].usr_stat = CHOOSE_ROOM;
            return;
        }        
    }
    else if(cli_set[t_fd].usr_stat == CHOOSE_ROOM)
    {
        send(t_fd, TA_WAIT, strlen(TA_WAIT), 0);
        return;
    }
    else if(cli_set[t_fd].usr_stat == IN_TALK)
    {
        // timer exit parttern
        // give a broadcast room / TIMER
        if(strcmp(EXIT_PAT, read_buf) == 0)
        {
            perror("Undefined: TA exited the room");
            exit(1);
        }
        else
        {
            broadcast_message(room_set, qst_set, t_fd, read_buf, cli_set);
        }
    }
    else if(cli_set[t_fd].usr_stat == QUESTION_ANSWERED)
    {
        // exit ta / close
    }
}

void student_handler(int s_fd, Cli* cli_set, Qst* qst_set, 
    const char* buffer, Rm* rm_set)
{
    if(cli_set[s_fd].usr_stat == ACTION_MENU)
    {
        if(strcmp(buffer, STUDENT_ASK_OPT) == 0) // STUDENT asked
        {
            send(s_fd, STUDENT_WHAT_QUESTION,
                 strlen(STUDENT_WHAT_QUESTION), 0);
            cli_set[s_fd].usr_stat = ASKED_QUESTION; 
        }
        if(strcmp(buffer, STUDENT_SPECT_OPT) == 0)
        {
            // list of open sessions. refer to description
        }
    }
    else if(cli_set[s_fd].usr_stat == ASKED_QUESTION)
    {
        add_question_set(buffer, s_fd, qst_set);
        cli_set[s_fd].usr_stat = AWAIT_RESPONSE;
        send(s_fd, STUDENT_WAIT, strlen(STUDENT_WAIT), 0);
    }
    else if(cli_set[s_fd].usr_stat == AWAIT_RESPONSE)
    {
        if(did_ta_accept(s_fd, qst_set) > 0)
        {
            // this will be handed from the ta point
            // make a broadcastroom for both of them
            send(s_fd, TA_ACCEPTED, strlen(TA_ACCEPTED), 0);
            cli_set[s_fd].usr_stat = CHOOSE_ROOM;
        }
        else
        {
            send(s_fd, STUDENT_WAIT, strlen(STUDENT_WAIT), 0);
        }
    }
    else if(cli_set[s_fd].usr_stat == CHOOSE_ROOM)
    {
        int check_room = student_chosen_room(buffer, s_fd,
            qst_set, rm_set);
        if(check_room == 0)
        {
            send(s_fd, ROOM_NOT_VALID, strlen(ROOM_NOT_VALID), 0);
            show_session_lists(s_fd, rm_set);
        }
        else
        {
            send(s_fd, NOW_IN_ROOM, strlen(NOW_IN_ROOM), 0);
            send(return_taf_from_stf(qst_set, s_fd), NOW_IN_ROOM,
                strlen(NOW_IN_ROOM), 0);
            sleep(1);
            send_pattern(s_fd, qst_set, check_room);
            send_pattern(return_taf_from_stf(qst_set, s_fd), 
                qst_set, check_room);
            cli_set[s_fd].usr_stat = IN_TALK;
            cli_set[return_taf_from_stf(qst_set, s_fd)].usr_stat = IN_TALK;
        }
    }
    else if(cli_set[s_fd].usr_stat == IN_TALK)
    {
        // room
        if(strcmp(buffer, EXIT_PAT) == 0)
        {
            cli_set[s_fd].usr_stat = QUESTION_ANSWERED;
            cli_set[return_taf_from_stf(qst_set, s_fd)].usr_stat = QUESTION_ANSWERED;
            send(s_fd, STUDENT_ANSWER, strlen(STUDENT_ANSWER), 0);
        }
        broadcast_message(rm_set, qst_set, s_fd, buffer, cli_set);
    }
    else if(cli_set[s_fd].usr_stat == WATCHING_SESSION)
    {
        // WATCHING SESSION
    }
    else if(cli_set[s_fd].usr_stat == QUESTION_ANSWERED)
    {
        // MENU
        // CLOSE QUESTION
        // CLOSE ROOM
        // SAVE QUESTION AND ANSWER INTO FILE
    }
}

void add_question_set(const char* buffer,int cli_fd, Qst* qst_set)
{
    strcpy(qst_set[qID].question, buffer);
    // +
    // char temp[300];
    // sprintf(temp, "Your Question is: %s\nIs it correct?", qst_set[qID].question);
    // send(cli_fd, temp, strlen(temp), 0);
    // +
    qst_set[qID].qst_id = qID;
    qst_set[qID].answer = NULL;
    qst_set[qID].st_fd = cli_fd;
    qst_set[qID].ta_fd = -1;
    qst_set[qID].q_stat = NOT_CHOSEN;
    qID++;
}

int did_ta_accept(int std_fd, Qst* qst_set)
{
    for(int i = 0; i < qID; i++)
    {
        if(std_fd == qst_set[i].st_fd &&
            qst_set[i].q_stat == IN_SESSION)
        {
            return 1;
        }
    }
    return 0;
}

int show_question_set(int client_fd, Qst* qst_set)
{
    int flag = 0;
    if(qID <= 0)
    {
        send(client_fd, NO_QUESTION, strlen(NO_QUESTION), 0);
        // sleep(0.5);
        send(client_fd, TA_MENU, strlen(TA_MENU), 0); // show TA_MENU again
        return 0;
    }
    else
    {
        for(int i = 0; i < qID; i++)
        {
            if(qst_set[i].q_stat == NOT_CHOSEN)
            {    
                char each_question[QUESTION_LENGTH];
                memset(each_question, 0, sizeof(each_question)/sizeof(each_question));
                sprintf(each_question, "Question #%d:\n%s\nStudent:#%d\n%s",
                    qst_set[i].qst_id, qst_set[i].question, qst_set[i].st_fd,
                    DELIM);
                send(client_fd, each_question, strlen(each_question), 0); // check for qst stat
                flag = 1;
            }
        }
        send(client_fd, END_OF_QUESTIONS, strlen(END_OF_QUESTIONS), 0);
    }
    if(flag == 0)
    {
        send(client_fd, NO_QUESTION, strlen(NO_QUESTION), 0);
        // sleep(0.5);
        send(client_fd, TA_MENU, strlen(TA_MENU), 0); // show TA_MENU again
        return 0;
    }
    return 1; // on success
}

void assign_ta_student(Cli* cli_set, const char* buffer, int t_fd, 
    Qst* qst_set, int* ta_student) // first is ta and second is student
{
    int qst_chosen = atoi(buffer);
    int ta;
    int student;
    if(qst_chosen > qID)
    {
        return;
    }
    for(int i = 0; i < qID; i++)
    {
        if(qst_set[i].qst_id == qst_chosen)
        {
            qst_set[i].q_stat = IN_SESSION;
            qst_set[i].ta_fd = t_fd;
            cli_set[t_fd].usr_stat = CHOOSE_ROOM;
            cli_set[qst_set[i].st_fd].usr_stat = CHOOSE_ROOM; // user in talk 
            ta = t_fd;
            student = qst_set[i].st_fd;
        }
    }
    ta_student[0] = ta;
    ta_student[1] = student;
    return;
}

// void create_room(int ta_fd, int st_fd)
// {
//     struct sockaddr_in broad_room;
//     int broad_socket = socket(AF_INET, SOCK_DGRAM, 0);
//     // +
//     char temp[100];
//     sprintf(temp, "Room with fileD:%d created\n", broad_socket);
//     write(1, temp, strlen(temp));
//     // +
//     broad_room.sin_family = AF_INET;
//     // broad_room. to be continued get porty from user
// }

void show_session_lists(int rec_fd, Rm* room_set)
{
    for(int i = 0; i < MAX_ROOM; i++)
    {
        // sleep(0.3);
        char each_room[200];
        memset(room_set, 0, sizeof(each_room)/sizeof(each_room));
        if(room_set[i].rm_stat == USED)
        {
            sprintf(each_room, "Room #%d, %s\n%s", room_set[i].port, "is in use",
            DELIM);
            send(rec_fd, each_room, strlen(each_room), 0);
        }
        else
        {
            sprintf(each_room, "Room #%d, %s\n%s", room_set[i].port, "is free",
            DELIM);
            send(rec_fd, each_room, strlen(each_room), 0);
        }
    }
}

void init_rooms(Rm* room_set)
{
    for(int i = 0; i < MAX_ROOM; i++)
    {
        room_set[i].port = DEFAULT_SERVER_PORT + i+1;
        room_set[i].rm_stat = UNUSED;        
        room_set[i].qst_in_room = -1;
        room_set[i].rm_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(room_set[i].rm_socket_fd <= 0)
        {
            perror("room socket problem");
        }
        room_set[i].rm_broadcast = (struct sockaddr_in*)malloc(
            sizeof(struct sockaddr_in));
        setup_room_broadcast(room_set[i].rm_broadcast, room_set[i].port,
            room_set[i].rm_socket_fd);
    }
}

void setup_room_broadcast(struct sockaddr_in* sockadr, int port, 
    int sock)
{
    int broadcast = 1;
    sockadr->sin_family = AF_INET;
    sockadr->sin_port = htons(port);
    sockadr->sin_addr.s_addr = inet_addr(SERVER_BROADCAST);
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &broadcast,
        sizeof(broadcast)))
    {
        perror("set room socket problem");
    }
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
        sizeof(broadcast)))
    {
        perror("set room socket problem");
    }
    if(bind(sock, (const struct sockaddr*)sockadr,
         sizeof(*sockadr)))
    {
        perror("error binding room");
    }
}

int student_chosen_room(const char* buffer, int s_fd, Qst* qst_set,
    Rm* rm_set)
{
    
    int room_port = atoi(buffer);
    if(room_port == 0)
    {
        return 0;
    }
    Rm* chosen_room = NULL;
    chosen_room = chosen_room_available(rm_set, room_port);
    if(chosen_room == NULL) 
    {
        return 0;
    }
    else
    {
        chosen_room->rm_stat = USED;
        chosen_room->student_fd = s_fd;
        chosen_room->ta_fd = return_taf_from_stf(qst_set, s_fd);
        chosen_room->qst_in_room = return_qID_from_fd(s_fd, qst_set);
        return room_port;
    }
}

Rm* chosen_room_available(Rm* rm_set, int rm_num)
{
    for(int i = 0; i < MAX_ROOM; i++)
    {
        if(rm_set[i].port == rm_num && rm_set[i].rm_stat == UNUSED)
        {
            return &rm_set[i];
        }
    }
    return NULL;
}

int return_taf_from_stf(Qst* q_set, int sfd)
{
    for(int i = 0; i <  MAX_QUESTION; i++)
    {
        if(q_set[i].st_fd == sfd &&
            q_set[i].q_stat == IN_SESSION)
        {
            return q_set[i].ta_fd;
        }
    }
    return -1;
}

int return_qID_from_fd(int fd, Qst* q_set)
{
    for(int i = 0; i < MAX_QUESTION; i++)
    {
        if((q_set[i].st_fd == fd || q_set[i].ta_fd == fd)
        && q_set[i].q_stat == IN_SESSION)
        {
            return q_set[i].qst_id;
        }
    }
    return -1;
}

void send_pattern(int s_fd, Qst* q_set, int port)
{
    char pat[100];
    sprintf(pat, "%s%d", PATTERN, port);
    send(s_fd, pat, strlen(pat), 0);
    send(return_taf_from_stf(q_set, s_fd), pat, strlen(pat), 0);
}

void set_rooms(fd_set* master, Rm* rm_set)
{
    for(int i = 0; i < MAX_ROOM; i++)
    {
        FD_SET(rm_set[i].rm_socket_fd, master);
    }
}

Rm* return_room_from_fd(int fd, Rm* rm_set)
{
    Rm* room;
    for(int i = 0; i < MAX_ROOM; i++)
    {
        if(rm_set[i].student_fd == fd || rm_set[i].ta_fd == fd)
        {
            room = &rm_set[i];
            return room;
        }
    }
    return NULL;
}

enum Role return_role_from_fd(int fd, Cli* cli_set)
{
    for(int i = 0; i < MAX_CLIENT; i++)
    {
        if(cli_set[i].user_fd == fd)
        {
            return cli_set[i].usr_type;
        }
    }
}

void broadcast_message(Rm* rm_set, Qst* q_set, int fd, const char* buffer, 
    Cli* cli_set)
{
    Rm* broadcast_room = return_room_from_fd(fd, rm_set);
    char b_message[BUFFER_SIZE+20];
    memset(b_message, 0, BUFFER_SIZE+20);
    enum Role fd_role = return_role_from_fd(fd, cli_set);
    if(fd_role == STUDENT)
    {
        sprintf(b_message, "Student#%d said: %s", fd, buffer);
    }
    else if(fd_role == TA)
    {
        sprintf(b_message, "TA#%d said: %s", fd, buffer);
    }
    else
    {
        perror("not legal");
    }
    sendto(broadcast_room->rm_socket_fd, b_message, strlen(b_message),
        0, (const struct sockaddr*) broadcast_room->rm_broadcast,
        (socklen_t)sizeof(*broadcast_room));
}
