A fork of the [par2cmdline-turbo](https://github.com/animetosho/par2cmdline-turbo) that added append feature similar to [MultiPar](https://github.com/Yutaka-Sawada/MultiPar/releases).

Thanks GLM-5 and GPT 5.4 ^_^

## Usage

create recovery record
```
par2 c --append test.7z
```
par2 will append the par2 data to the 7z file

verify recovery record
```
par2 v --appended test.7z
```

repair the 7z file
```
par2 r --appended test.7z
```
a repaired file named `test.7z.repaired` will be created.
