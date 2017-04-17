from random import randint, gauss

N = 20

for i in range(N):
    q = randint(0, 1)
    if q == 0: print("alloc " + str(int(gauss(100, 100))))
    else : print("free " + str(randint(100, 2000)))

print("end")
