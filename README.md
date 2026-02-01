# Messagerie-TCP

# Messagerie TCP (C) — mini serveur multi-clients

Projet de programmation réseaux : un serveur TCP multi-clients (threads) qui gère des **pseudos** et permet d’**envoyer/recevoir** des messages entre utilisateurs connectés.

> Testable facilement avec `telnet` / `nc` (netcat).

---

## Fonctionnalités

- Serveur TCP **multi-clients** (1 thread par client)
- Attribution d’un pseudo (`alias`)
- Liste des utilisateurs connectés (`list`)
- Envoi d’un message à un autre utilisateur (`send`)
- Réception des messages en attente (`recv`)
- Quelques commandes “TP” utiles : `echo`, `random`, `time`, `quit`

---

## Fichiers

- `ex5.c` : **serveur messagerie** (alias / list / send / recv)
- `ex4.c` : étape précédente (liste de clients)
- `serverCom.c` : serveur à commandes (echo/random/time/quit)
- `serverMulti.c` : serveur multi-clients (base threads)
- `serverEcho.c` : serveur TCP simple (début)

---

## Compilation

### Serveur messagerie (recommandé)
```bash
gcc -Wall -Wextra -O2 -pthread ex5.c -o server
