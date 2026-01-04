#ifndef PROTOCOL_H
#define PROTOCOL_H

#define SERVER_PORT 5000

/*
PROTOCOLE TCP (simple)

Client → Serveur :
    (aucune donnée, simple demande)

Serveur → Client :
    int32_t nb_process (network byte order)
    tableau de process_t [nb_process]

Chaque process_t est envoyé tel quel (structure binaire)
*/

#endif