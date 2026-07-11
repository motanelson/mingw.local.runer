print("\033c\033[47;31m\ngive me a utf-32 .bin utf-32 file to encript ? ")
a=input().strip()
b=a.replace(".bin","")
f1=open(a,"r",encoding="utf-32")
f=f1.read()
f1.close()

counter=0

f1=open(b+".txt","w",encoding="utf-8")
f1.write(f)
f1.close()