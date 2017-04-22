#include "wrapper.h"
#include <stdio.h>
#include <string.h>

/* Kod funkcii my_alloc a my_free nahradte vlastnym. Nepouzivajte ziadne
 * globalne ani staticke premenne; jedina globalna pamat je dostupna pomocou
 * mread/mwrite/msize, ktorych popis najdete vo wrapper.h */

/* Ukazkovy kod zvladne naraz iba jedinu alokaciu. V 0-tom bajte pamate si
 * pamata, ci je pamat od 1 dalej volna alebo obsadena. 
 *
 * V pripade, ze je volna, volanie my_allloc skonci uspesne a vrati zaciatok
 * alokovanej RAM; my_free pri volnej mamati zlyha.
 *
 * Ak uz nejaka alokacia prebehla a v 0-tom bajte je nenulova hodnota. Nie je
 * mozne spravit dalsiu alokaciu, takze my_alloc musi zlyhat. my_free naopak
 * zbehnut moze a uvolni pamat.
 */

typedef struct block_info {
    int next; /* ukazovatel na predchadzajuci block */
    char free; /* flag o tom, ci je block volny (8-bitov staci) */
} preamble;


preamble read_preamble(int pos) {
    int p_size = sizeof(preamble);
    char buffer[p_size];
    preamble p;

    for (int i = pos; i < pos + p_size; i++) {
        buffer[i - pos] = mread(i);
    }
    memcpy(&p, buffer, p_size);
    return p;
}


void write_preamble(preamble p, int pos) {
    int p_size = sizeof(preamble);
    int m_size = msize();
    char buffer[p_size];
    memcpy(buffer, &p, p_size);

    for (int i = pos; i < pos + p_size; i++) {
        mwrite(i, buffer[i - pos]);
    }
}


void merge(preamble *a, preamble *b) {
    a->next = b->next;
}


/**
 * Inicializacia pamate
 *
 * Zavola sa, v stave, ked sa zacina s prazdnou pamatou, ktora je inicializovana
 * na 0.
 */
void my_init(void) {
    int p_size = sizeof(preamble);
    int m_size = msize();
    /* ak nemam dostatok priestoru pre 2 preambuly */ 
    if (m_size < 2*p_size) return;
    /* zaciatok */
    preamble init;
    init.free = 1;
    init.next = m_size - p_size;

    /* koniec */
    preamble term;
    term.next = -1;
    term.free = 0;

    /* zapisem mantinely do pamate */
    write_preamble(init, 0);
    write_preamble(term, init.next);
    /* fprintf(stderr, "%s\n", "INITIALIZATION FINISHED"); */
}

/**
 * Poziadavka na alokaciu 'size' pamate. 
 *
 * Ak sa pamat podari alokovat, navratova hodnota je adresou prveho bajtu
 * alokovaneho priestoru v RAM. Pokial pamat uz nie je mozne alokovat, funkcia
 * vracia FAIL.
 */
int my_alloc(unsigned int size) {
    int p_size = sizeof(preamble); 
    int m_size = msize();
/*    fprintf(stderr, "chcem alokovat: %d\n", (int)size); */
    
    /* solution for small memory space */
    if (m_size == 1) return -1;
    if (m_size < 2*p_size) {
        if (mread(0) == 1) return FAIL;
        else { mwrite(0, 1); return 1; }
    }

    /* ak ziadam vacsiu pamat ako mam dokopy alebo nieco nekladne */
    if (((int)size > m_size - 2*p_size) || ((int)size <= 0)) return FAIL;

    preamble act_block;
    int offset = 0, act_size = -1, act_addr = -1;
    char found_suitable_block = 0;
    do {
        /* nacitam preambulu */
        act_block = read_preamble(offset);
        act_addr = offset + p_size;
        act_size = (act_block.next == -1 ? m_size-1 : act_block.next) - (offset + p_size);
        /* if (act_addr == 0) fprintf(stderr, "%d\n", act_size); */

        if (act_block.free && act_size >= (int)size) {
            found_suitable_block = 1;
            break;
        }
        offset = act_block.next;
        if (m_size - p_size - offset - 1 < (int)size) break;
    } while (act_block.next != -1);

    /* ak som nenasiel vhodny blok, vratim FAIL */
    if (!found_suitable_block) return FAIL;

    /* ak mi ostalo dost pamate vytvorim doplnujuci block */
    if ((int)(act_size - size - p_size) > 0) {
        preamble new_block;
        new_block.free = 1;
        new_block.next = act_block.next;
        int new_addr = (int)(act_addr + size + p_size);
        write_preamble(new_block, new_addr - p_size);

         /* useknem stary block */
        act_block.next = act_addr + size;
    }
    act_block.free = 0;
    write_preamble(act_block, offset);
    
    /* vratim zaciatok alokovanej pamate */
    return act_addr;
}

/**
 * Poziadavka na uvolnenie alokovanej pamate na adrese 'addr'.
 *
 * Ak bola pamat zacinajuca na adrese 'addr' alokovana, my_free ju uvolni a
 * vrati OK. Ak je adresa 'addr' chybna (nezacina na nej ziadna alokovana
 * pamat), my_free vracia FAIL.
 */

int my_free(unsigned int addr) {
    int p_size = sizeof(preamble);
    int m_size = msize();

    /* solution for small memory space */
    if (m_size == 1) return FAIL;
    if (m_size < 2*p_size) {
        if (mread(0) == 1) { mwrite(0, 0); return OK; }
        else return FAIL;
    }

    if ((addr >= m_size - p_size)  || ((int)addr < 0)) return FAIL;
    
    preamble act_block, prev_block;
    int offset = 0, act_addr = -1, prev_addr = -1;
    char is_valid = 0;
    do {
        /* nastavim prev */
        prev_block = act_block;
        prev_addr = act_addr;

        act_block = read_preamble(offset);
        act_addr = offset + p_size;
        if (act_addr == (int)addr && act_block.free == 0) {
            is_valid = 1;
            break;
        }
        offset = act_block.next;
        /* ak uz nemozem najst vhodny block */
        if (offset >= (int)addr) break;
   } while (act_block.next != -1);

    if (!is_valid) return FAIL;
    act_block.free = 1;

    /* ak je predchadzajuci block free, mergni */
    if (prev_addr != -1) {
        if (prev_block.free) {
            merge(&prev_block, &act_block);
            offset = prev_addr - p_size;
            act_block = prev_block;
       }
    }
    /* ak je nasledujuci block free, mergni */
    if (act_block.next != -1) {
        preamble next_block = read_preamble(act_block.next);
        if (next_block.free) merge(&act_block, &next_block);
    }
    write_preamble(act_block, offset);

    return OK;
}
