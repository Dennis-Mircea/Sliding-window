#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

int min(int param1, int param2) {
  if (param1 < param2) {
    return param1;
  }
  return param2;
}
int max(int param1, int param2) {
  if (param1 > param2) {
    return param1;
  }
  return param2;
}

void writeTo(int fd_received, int length, msg* buffer) {
  int data_write = length; // lungimea datelor trebuiesc scrise
  int dim = length / 1395 + 1; // numarul de mesaje care trebuiesc scrise

  for (int i = 0; i < dim; i++) {
    int count = 0;

    // daca lungimea datelor care trebuiesc scrie este mai mica decat 1395
    if (data_write < 1395) {
      // scrie in fisier
      while (count < data_write - 1) {
        int result = write(fd_received, buffer[i].payload + count, data_write - 1 - count);
        count += result;
      }
    } else {
      // scrie in fisier
      while (count != 1395) {
        int result = write(fd_received, buffer[i].payload + count, 1395 - count);
        count += result;
      }
    }
    // scadem datele pe care le-am scris deja in fisier
    data_write -= count;
  }
}

// functie care verfica daca un mesaj e corupt
int checkCorrupt(msg t) {
	char xors_byte = t.payload[0];
	for (int i = 1; i < 1399; ++i) {
		xors_byte ^= t.payload[i];
	}
	if (xors_byte == t.payload[1399]) {
		return 1;
	} else {
		return -1;
	}
}

int main(int argc,char** argv){
  msg r,t;
  init(HOST,PORT);

  int ack = 1;
  int nak = -1;

  // primirea numelui fisierului, a lungimii ferestrei si a lungimii fisierului
  int result = recv_message(&r);  

  if (result < 0) {
  	perror("Recvier error");
  	return -1;
  }

  // crearea si deschiderea fisierului in care vor fi scrise datele primite
  char* name = malloc((5 + r.len) * sizeof(char));
  name = strcpy(name, "recv_");
  int fd_received = creat(strcat(name, r.payload), S_IRUSR | S_IWUSR);

  int length, WINDOW;
  memcpy(&length, r.payload + r.len, sizeof(int));
  memcpy(&WINDOW, r.payload + sizeof(int) + r.len, sizeof(int));

  // trimitem ACK pentru ca am primit mesajul corect
  memcpy(t.payload, &ack, sizeof(int));

  send_message(&t);

  // calcularea numarului de mesaje
  int dim = length / 1395 + 1;

  // alocarea bufferului in care vor fi memorate mesajele pentru a fi
  // scrise in fisierul de output
  msg *buff = malloc(dim * sizeof(msg));

  // fereastra glisanta prin care asteapta fiecare pachet
  int *sliding_window = malloc(dim * sizeof(int));
  for (int i = 0; i < dim; ++i) {
    sliding_window[i] = -1;
  }
  // contori pentru fereastra glisanta
  int begin = 0;
  int end = min(WINDOW - 1, dim - 1);

  int nr_cadru; // varibabila in care stocam numarul cadrului
  int expected = 0; // variabila care ne indica pachetul pe care-l asteptam
  // cat timp nu ajungem la final
  while (begin != end + 1) {
    result = recv_message(&r);

    // verificam daca mesajul nu este corupt
    if (checkCorrupt(r) == 1) {
    	// verificam daca numarul cadrului primit corespunde cu cel asteptat
    	memcpy(&nr_cadru, r.payload + r.len, sizeof(int));
	    if (expected == nr_cadru) {

	    	// memoram mesajul primit in buffer
	      memcpy(buff[nr_cadru].payload, r.payload, r.len);
	      buff[nr_cadru].len = r.len;

	      // trimitem ack ca am primit mesajul
	    	memcpy(t.payload, &ack, sizeof(int));
  			memcpy(t.payload + sizeof(int), &nr_cadru, sizeof(int));

  			send_message(&t);

	      // punem cadrul in fereastra
	      sliding_window[nr_cadru] = nr_cadru;

	      // verificam daca putem avansa cu fereastra glisanta
  			while (sliding_window[begin] != -1 && begin != end + 1) {
          // daca start nu a ajuns la end, avanseaza cu o pozitie
          if (begin <= end) {
            begin += 1;
          }
          // daca end n-a ajuns la sfarsit de fisier, avanseaza cu o pozitie
          if (end < dim - 1) {
            end += 1;
          }
  			}

	      // vom astepta urmatorul mesaj
	      expected++;
	    // daca primim un cadru mai mic decat cel asteptat, inseamna ca este un pachet
	    // pentru care am trimis nak
	    } else if (expected < nr_cadru) {
	    	
	    	// memoram mesajul in buffer
	    	memcpy(buff[nr_cadru].payload, r.payload, r.len);
	    	buff[nr_cadru].len = r.len;


	    	// punem cadrul in fereastra
      	sliding_window[nr_cadru] = nr_cadru;

      	// pentru toate cadrele pana in pachetul primit trimitem nak
      	for (int j = expected; j < nr_cadru; j++) {
      		memcpy(t.payload, &nak, sizeof(int));
				  memcpy(t.payload + sizeof(int), &j, sizeof(int));

				  send_message(&t);
      	}

	      // trimitem ack ca am primit mesajul
	    	memcpy(t.payload, &ack, sizeof(int));
  			memcpy(t.payload + sizeof(int), &nr_cadru, sizeof(int));

  			send_message(&t);

  			// urmatorul pachet pe care-l vom astepta este ce-l de dupa pachetul pe care l-am primit anterior
	      	expected = nr_cadru + 1;
	    } else {
	    	// memoram mesajul primit in buffer
	      	memcpy(buff[nr_cadru].payload, r.payload, r.len);
	      	buff[nr_cadru].len = r.len;

	      	// trimitem ack ca am primit mesajul
  	    	memcpy(t.payload, &ack, sizeof(int));
    			memcpy(t.payload + sizeof(int), &nr_cadru, sizeof(int));

    			send_message(&t);

  	    	// punem cadrul in fereastra
  	    	sliding_window[nr_cadru] = nr_cadru;

  	    	// verificam daca putem avansa cu fereastra glisanta
    			while (sliding_window[begin] != -1 && begin != end + 1) {
            // daca start nu a ajuns la end, avanseaza cu o pozitie
            if (begin <= end) {
              begin += 1;
            }
            // daca end n-a ajuns la sfarsit de fisier, avanseaza cu o pozitie
            if (end < dim - 1) {
              end += 1;
            }
    			}
	    }
	}
  }

  // scriem in fisierul de output toate datele din buffer
  writeTo(fd_received, length, buff);

  // inchidem fisierul
  close(fd_received);
  
  // trimitem "FIN" pentru ca sa anuntam senderul ca am primit tot ce asteptam
  sprintf(t.payload,"FIN");
  t.len = strlen(t.payload + 1);
  send_message(&t);
  return 0;
}
