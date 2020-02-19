# Sliding-window
 Implemented a sliding window by sending data from a file from a path to a receiver

In implementarea temei am trimis initial numele fisierului, lungimea
fisierului(pentru care am implementat o functie), cat si lungimea ferestrei
glisante.
	Dupa primirea acestor parametrii am creat fisierul de output in care se
vor scrie mesajele primite.

	--- CITIRE/SCRIERE ---
	Am folosit 2 buffere, atat pentru citirea datelor din fisier, cat si pentru
scrierea mesajelor in fisierele de output. Datele din fisierul de input le-am
impartit in mai multe mesaje grupandu-le in buffer, fiecare mesaj cu payloadul
de 1400, dintre care 1395 in care va fi continut mesajul propriu-zis, 4 pentru
numarul cadrului respectiv si 1 pentru caracterul de verificare a secventei
pentru a vedea daca aceasta se va corupe sau nu.
	
	---- FEREASTRA GLISANTA ----
	Pentru crearea ferestrei glisante, am alocat un vector de mesaje cat
dimensiunea numarului de mesaje(dim / 1395) care va retine mereu in sender
ce mesaje s-au trimis corect, adica pentru care a primit ack, si mesajele
pentru care a primit nak. La fel si in reciever, acest vector va retine ce
mesaje s-au primit si ce mesaje nu. Cu ajutorul a doi contor (begin si end)
vom stii mereu unde suntem cu fereastra glisanta si ce mesaje asteptam.

	---- FUNCTIONALITATE ----
	Pentru functionarea ferestrei glisante, in prima faza se vor trimite window
mesaje de la sender catre reciever, iar apoi recieverul va incepe sa trimita
ack-uri sau nak-uri. Sender-ul, dupa ce a trimis window mesaje, asteapta raspuns
de la reciever.
	Daca a primit ACK, senderul trimite mai departe urmatorul cadru,
care este memorat in buffer, iar daca a primit "NAK nr_cadru", va retrimite
nr_cadru. Recieverul, pe cealalta parte, cat timp primeste pachetul pe care il
asteapta, va trimite ACK, iar daca nu a primit ce astepta, va trimite NAK pe
toate pachete pe care nu le-a primit pana in pachetul respectiv si ACK pe pachetul
pe care l-a primit, asta in cazul in care a primit un cadru mai mare decat pe
cel pe care-l astepta. In cazul in care a primit un cadru mai mic decat pe cel
ce il astepta, nu va face nimic, il va pune doar in bufferul de memorie.
	Toate pachetele primite se vor pune in buffer. Fereastra glisanta va avansa
(contoarele se vor incrementa) doar daca se va primi pachetul de la contorul
begin.
	Exista posibilitatea ca la reciever sa nu ajunga nimic, iar acesta sa nu
dea niciun raspuns, iar in acest caz, se va atinge timeout ul in sender, iar
acesta v-a retrimite toate mesajele pentru care nu a primit raspuns sau a 
primit nak.

	---- CORUPERE ----
	Pentru coruperea datelor, la fel cum am zis si la inceput, se va pune la
sfarsitul payload-ul mesajului un caracter cu un xor pe toti bitii din acesta.
Pentru toata aceasta operatie, am implementat o functie in sender si o functie
in reciever care verifica daca caracterul memorat la sfarsit este egal cu
xorul pe toti bitii din payload. Daca nu este egal, inseamna ca mesajul este
corupt, iar recieverul nu va face nimic, astfel ca in sender se va atinge
timeout-ul si acesta va retrimite pachetul.

	---- REORDONARE ----
	Reordonarea v-a fi realizata de la sine, prin intermediul bufferelor si a
numarului de cadru al fiecarul pachet, care v-a asigura mereu o ordine.
