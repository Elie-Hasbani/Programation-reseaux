#  Oware Multiplayer Server – Guide d’utilisation

Ce serveur permet à plusieurs joueurs de se connecter, jouer à l’Awalé, lancer des défis, suivre des matchs et discuter.  
Chaque joueur peut :

- Se déconnecter **sans perdre ses parties**
- Avoir **plusieurs matchs en même temps**
- Ne jamais avoir **plus d’un match avec la même personne**
- Être spectateur d’autres parties (publiques ou privées)


#  Compilation
faire make dans le repertoire principal, puis ecrire ./trg/server pour lancer le serveur et ./trg/client [adresse] pour lancer un client

---

#  Connexion & Comptes

Lorsque vous vous connectez :

1. Le serveur demande si vous avez déjà un compte (`yes` / `no`)
2.  
   - **Si yes** : entrez votre pseudo et mot de passe  
   - **Si no** : choisissez votre nouveau pseudo + mot de passe

Un joueur peut se déconnecter à tout moment :  
 **Il restera dans la liste, ses parties continueront d’exister et il pourra les reprendre à son retour.**

---

#  Commandes disponibles

Toutes les commandes se tapent directement dans le client.


##  1. Voir la liste des joueurs

Affiche tous les joueurs inscrits :  
- en ligne  
- ou hors ligne

##  2. Voir la liste des matchs en cours
list games


##  3. Défi (challenge) un joueur


challenge NOM


##  4. Défi privé (avec liste de spectateurs autorisés)


challenge NOM private ami1 ami2 ami3 ...

##  5. Répondre à un défi
response NOM yes/
response NOM no

- `yes` → la partie commence  
- `no` → le défi est supprimé

##  6. Jouer un coup
play NOM pit

- `NOM` = nom de l’adversaire  
- `pit` = numéro de la case à jouer

Chaque joueur peut avoir **plusieurs matchs simultanés**, avec des adversaires différents.

##  7. Regarder (spectate) un match
spectate NOM1 NOM2

Regarde la partie entre deux joueurs.

- Fonctionne pour les matchs publics  
- Permet d’accéder aux matchs privés **uniquement si vous êtes autorisé**

##  8. Modifier sa bio
update bio votre texte ici

##  9. Voir la bio d’un joueur
show bio NOM

##  9. Chater avec un autre joueur

chat NOM votre message ici



