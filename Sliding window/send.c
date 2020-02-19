#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

// functii pentru max si min
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
// functie care calculeaza lungimea unui fisier
int getLength(int fd) {
  int end = lseek(fd, 0, SEEK_END);
  if (end < 0) {
    perror("Error end");
    return -1;
  }

  int start = lseek(fd, 0, SEEK_SET);
  if (start < 0) {
    perror("Error start");
    return -1;
  }
  return end - start + 1;
}

// functie care face xor pe toti bitii in payload si pune suma pe ultima pozitie
void corrupt(msg* t) {
  char xor_byte = t->payload[0];

  for (int i = 1; i < 1399; ++i) {
    xor_byte ^= t->payload[i];
  }

  t->payload[1399] = xor_byte;
}

// functie care citeste dintr-un fisier
void readFrom(int fd, int length, msg* buffer) {
  int data_read = length;
  int dim = length / 1395 + 1;
  
  // setam cu 0 toata zona din memorie
  memset(buffer, 0, dim * sizeof(msg));

  for (int i = 0; i < dim; i++) {
    int count = 0;

    if (data_read < 1395) {
      
      // citeste data_read din fisier
      while (count < data_read - 1) {
        int result = read(fd, buffer[i].payload + count, data_read - count);
        count += result;
      }
      buffer[i].len = data_read - 1; // seteaza lungimea
      memcpy(buffer[i].payload + count, &i, sizeof(int)); // pune numarul de secventa
      corrupt(&buffer[i]); // pune valoarea pentru coruperea mesajului
    } else {
      
      // citeste un mesaj de 1395 din fisier
      while (count != 1395) {
        int result = read(fd, buffer[i].payload + count, 1395 - count);
        count += result;
      }

      buffer[i].len = 1395; // seteaza lungimea
      memcpy(buffer[i].payload + count, &i, sizeof(int)); // pune numarul de secventa
      corrupt(&buffer[i]); // pune valoarea pentru coruperea mesajului
    }
    data_read -= count; // scade ce a citit
  }
}

int main(int argc,char** argv){
  init(HOST,PORT);
  msg t, r;

  // atribuirea parametrilor SPEED, DELAY si BDP
  int SPEED = atoi(argv[2]);
  int DELAY = atoi(argv[3]);
  int BDP = SPEED * DELAY;

  // calcularea ferestrei gisante
  int WINDOW = BDP * 1000 / (1404 * 8);
  
  // deschiderea fisierului din care vor fi citite datele
  int fd = open(argv[1], O_RDONLY);

  // aflarea lungimii fisierului
  int length = getLength(fd);

  // calcularea numarului de mesaje
  int dim = length / 1395 + 1;

  // trimiterea numelui fisierului, a lungimii fisierului si a ferestrei
  memset(&t, 0, sizeof(msg));
  memcpy(t.payload, argv[1], strlen(argv[1]));
  t.len = strlen(t.payload)+1;

  memcpy(t.payload + t.len, &length, sizeof(int));
  memcpy(t.payload + sizeof(int) + t.len, &WINDOW, sizeof(int));

  send_message(&t);
  int result = recv_message_timeout(&r, 2 * DELAY);
  
  while (result == -1) {
	  send_message(&t);
	  result = recv_message_timeout(&r, 2 * DELAY);
  }

  // alocarea bufferului in care vor fi stocate datele
  msg *buff = malloc(dim * sizeof(msg));

  // citim din fisier si punem in buffer
  readFrom(fd, length, buff);

  // fereastra glisanta care verifica toate datele pe care a primit ack
  int *sliding_window = malloc(dim * sizeof(int));
  for (int i = 0; i < dim; ++i) {
  	sliding_window[i] = -1;
  }

  // 2 contori care retin inceputul si sfarsitul ferestrei glisante
  int begin = 0;
  int end = min(WINDOW - 1, dim - 1);

  // verificam daca dimensiunea fisierului este mai mare decat fereastra glisanta
  if (dim > WINDOW) {
    // trimitem WINDOW mesaje
  	for (int i = 0; i < WINDOW; ++i) {
  		send_message(&buff[i]);
  	}

    // trimitem mesaje cat timp primim ack de dim - WINDOW ori
  	int i = WINDOW;
  	while (i != dim) {	
  		result = recv_message_timeout(&r, min(1000, 2 * DELAY));
  		if (result != -1) {
        // punem raspunsul recieverului in response
  			int response;
       		memcpy(&response, r.payload, sizeof(int));

        	// daca primim ack
        	if (response == 1) {
  				int nr_cadru; // variabila in care stocam numarul cadrului
  				memcpy(&nr_cadru, r.payload + sizeof(int), sizeof(int));

          		// punem cadrul in fereastra
  				sliding_window[nr_cadru] = nr_cadru;
          		// daca primul cadru s-a primit si nu am ajuns la sfarsit, fereastra avanseaza
  				while (sliding_window[begin] != -1 && begin != end + 1) {
            		// daca start nu a ajuns la end, avanseaza cu o pozitie
  					if (begin <= end) {
              			begin += 1;
            		}
           			// daca end n-a ajuns la sfarsit de fisier, avanseaza cu o pozitie
            		if (end < dim - 1) {
              			end += 1;
            		}

            		// cat timp nu am ajung la sfarsitul fisierului trimitem urmatorul mesaj
  					if (i < dim) {
	  					send_message(&buff[i]);
	  					i++;
  					}
  				}
        	// daca primim nak retrimitem cadrul
  			} else {
  				int nr_cadru;
  				memcpy(&nr_cadru, r.payload + sizeof(int), sizeof(int));
  				send_message(&buff[nr_cadru]);
  			}
  		} else {
        // daca primim timeout, retrimitem toate mesajele din fereastra pe care n-am primit ack
  			for (int j = begin; j <= end ; j++) {
	  			if (sliding_window[j] == -1) {
	  				send_message(&buff[j]);
	  			}
  			}
  		}
  	}

  	while (begin != end + 1) {
  		result = recv_message_timeout(&r, min(1000, 2 * DELAY));
      	// verifica data am primit timeout
  		if (result != -1) {
  			// punem raspunsul recieverului in response
	        int response;
	        memcpy(&response, r.payload, sizeof(int));

	        // daca primim ack
	        if (response == 1) {
  				int nr_cadru; // variabila in care stocam numarul cadrului
  				memcpy(&nr_cadru, r.payload + sizeof(int), sizeof(int));

          		// punem cadrul in fereastra glisanta
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
        	// daca primim nak retrimitem cadrul
  			} else {
  				int nr_cadru;
  				memcpy(&nr_cadru, r.payload + sizeof(int), sizeof(int));
  				send_message(&buff[nr_cadru]);
  			}
  		} else {
        // daca primim timeout, retrimitem toate mesajele din fereastra pe care n-am primit ack
  			for (int j = begin; j <= end ; ++j) {
	  			if (sliding_window[j] == -1) {
	  				send_message(&buff[j]);
	  			}
  			}

  		}
  	}

  } else {
    // daca lungimea ferestrei glisante este mai mare decat lungimea fisierului
  	for (int i = 0; i < dim; ++i) {
  		send_message(&buff[i]);
  	}
  	int i = 0;
    while (i != dim) {  
      	result = recv_message_timeout(&r, min(1000, 2 * DELAY));
      	if (result != -1) {
        // punem raspunsul recieverului in response
        int response;
        memcpy(&response, r.payload, sizeof(int));

        // daca primim ack
        if (response == 1) {
          	int nr_cadru; // variabila in care stocam numarul cadrului
          	memcpy(&nr_cadru, r.payload + sizeof(int), sizeof(int));

          	// punem cadrul in fereastra
          	sliding_window[nr_cadru] = nr_cadru;
          	// daca primul cadru s-a primit si nu am ajung la sfarsit, fereastra avanseaza
          	while (sliding_window[begin] != -1 && begin != end + 1) {
	            // daca start nu a ajuns la end, avanseaza cu o pozitie
	            if (begin <= end) {
	              begin += 1;
	            }
	            // daca end n-a ajuns la sfarsit de fisier, avanseaza cu o pozitie
	            if (end < dim - 1) {
	              end += 1;
	            }

	            // cat timp nu am ajung la sfarsitul fisierului trimitem urmatorul mesaj
	            if (i < dim) {
	              send_message(&buff[i]);
	              i++;
	            }
          	}
        // daca primim nak retrimitem cadrul
        } else {
          	int nr_cadru;
          	memcpy(&nr_cadru, r.payload + sizeof(int), sizeof(int));
          	send_message(&buff[nr_cadru]);
        }
      } else {
        // daca primim timeout, retrimitem toate mesajele din fereastra pe care n-am primit ack
        for (int j = begin; j <= end ; j++) {
          if (sliding_window[j] == -1) {
            send_message(&buff[j]);
          }
        }
      }
    }
  }
  
  close(fd);
  // asteapta primirea mesajului "FIN" de la reciever pentru a incheia conexiunea
  result = recv_message_timeout(&t, 2 * DELAY);
  while (strstr(t.payload, "FIN") == NULL || result == -1) {
  	result = recv_message_timeout(&t, 2 * DELAY);
  }
  return 0;
}
