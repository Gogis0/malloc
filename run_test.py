import random
from subprocess import Popen, PIPE

mem_size = 5000
queries = 100

segments = []
allocated, allocations = 0, 0

def main():
    global allocated, allocations
    p = None
    for q in range(queries):
        q_type = random.randint(0, 1)

        if q_type == 0:
            p = Popen(["./alloc.o", str(mem_size)], stdin=PIPE, stdout=PIPE, encoding="UTF-8")
            kolko = 0
            while kolko == 0: kolko = str(abs(int(random.gauss(100, 100))))
            cmd = "alloc " + str(kolko) + " end"
            #print("command:", cmd)
            kde = p.communicate(cmd)[0].rstrip()

            if kde != "-1":
                print("ALLOCATED: {:>7} on addr {}".format(kolko, kde))
                segments.append((kolko, kde))
                allocated += 1
            else:
                print("couln't allocate:", kolko)
            allocations += 1
        else:
            if len(segments) == 0: continue
            p = Popen(["./alloc.o", str(mem_size)], stdin=PIPE, stdout=PIPE, encoding="UTF-8")
            co = random.randint(0, len(segments)-1)
            cmd = "free " + segments[co][1] + " end"
            #print("command:", cmd)

            out = p.communicate(cmd)[0].rstrip()

            if out == "0":
                print("FREED: {:>11} on addr {}".format(segments[co][0], segments[co][1]))
                segments.pop(co)
            else:
                print("couldn't free", segments[co][0], "on addr:", segments[co][1])

    print("allocated/allocations = ", allocated/allocations)
    print("allocated = ", allocated, "allocations = ", allocations)



if __name__ == "__main__":
    main()
