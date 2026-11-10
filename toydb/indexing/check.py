#!/usr/bin/env python3
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import re


def main():
    root=Path(__file__).resolve().parent.parent
    work=Path(tempfile.mkdtemp(prefix="toydb-index-check-"))
    print(f"Phase 3 sources, data and logs: {work}", flush=True)
    for layer in ("pflayer","rmlayer","amlayer","indexing"):
        (work/layer).mkdir()
        for p in (root/layer).iterdir():
            if p.suffix in (".c",".h") or p.name.lower()=="makefile":shutil.copy2(p,work/layer/p.name)
    flags=os.environ.get("INDEX_CFLAGS","-std=gnu89 -g -Wall -Wextra")
    with (work/"build.log").open("w") as log:
        for layer,target in (("pflayer","pflayer.o"),("rmlayer","rmlayer.o"),("rmlayer","student.o"),("amlayer","amlayer.o"),("indexing","test_index"),("indexing","test_am")):
            r=subprocess.run(["make","-B","-C",str(work/layer),target,"CFLAGS="+flags],stdout=log,stderr=subprocess.STDOUT)
            if r.returncode:raise RuntimeError(f"build failed: {work/'build.log'}")
    for frames,policy in ((1,0),(2,1),(3,0),(3,1)):
        run=work/f"am-{frames}-{policy}";run.mkdir()
        r=subprocess.run([str(work/"indexing/test_am"),str(frames),str(policy)],cwd=run,capture_output=True,text=True,timeout=90)
        (work/f"am-{frames}-{policy}.log").write_text(r.stdout+r.stderr)
        if r.returncode or "runtime error:" in r.stderr or "ERROR: AddressSanitizer" in r.stderr:raise RuntimeError(r.stdout+r.stderr)
    print("PASS: four AM ownership/error/duplicate-bound regressions",flush=True)
    cases=[]
    for method in (0,1,2):
        for n,length,order,dup in ((0,8,0,0),(1,8,0,0),(62,8,0,0),(63,8,0,0),(9,250,0,0),(180,8,1,0),(200,8,2,2),(30,8,3,0),(500,250,2,0)):
            for frames,policy in ((3,0),(3,1)):
                cases.append((method,n,length,order,dup,frames,policy,int(method==2 and n in (9,62,63)),0))
    for n,length in ((180,8),(500,250)):
        for policy in (0,1):cases.append((2,n,length,0,0,1,policy,0,0))
    for number,args in enumerate(cases):
        run=work/f"run-{number}";run.mkdir()
        result=subprocess.run([str(work/"indexing/test_index"),*map(str,args)],cwd=run,text=True,capture_output=True,timeout=90)
        (work/f"run-{number}.log").write_text(result.stdout+result.stderr)
        if result.returncode or "runtime error:" in result.stderr or "ERROR: AddressSanitizer" in result.stderr:raise RuntimeError(f"case {args}: {result.stdout}{result.stderr}")
    print(f"PASS: {len(cases)} construction/lookup/range/reopen cases",flush=True)
    # Original result sequences are checked, independent of the legacy main exit.
    expected={1:[list(range(26)),list(range(50))],2:[sorted(range(100),key=str)],3:[list(range(0,20,2)),[],[],list(range(100,151))]}
    for n in (1,2,3):
        command=["cc",*flags.split(),"-I"+str(work/"amlayer"),"-o",str(work/f"test{n}"),str(work/f"amlayer/test{n}.c"),str(work/"amlayer/misc.c"),str(work/"amlayer/amlayer.o"),str(work/"pflayer/pflayer.o")]
        r=subprocess.run(command,capture_output=True,text=True)
        if r.returncode:raise RuntimeError(r.stderr)
        run=work/f"original-{n}";run.mkdir()
        r=subprocess.run([str(work/f"test{n}")],cwd=run,capture_output=True,text=True,timeout=60)
        (work/f"original-{n}.log").write_text(r.stdout+r.stderr)
        groups=[];current=[]
        for line in r.stdout.splitlines():
            if re.fullmatch(r"\d+",line):current.append(int(line))
            elif line.startswith("retrieved "):groups.append(current);current=[]
        if groups!=expected[n] or f"test{n} done!" not in r.stdout:raise RuntimeError(f"original test{n} wrong output")
        print(f"PASS: original AM test{n} output verified (legacy exit {r.returncode})",flush=True)


if __name__=="__main__":main()
