# reference/

The Epitech reference server (`zappy_ref-v3.0.1.tgz`) lives **outside** the repo and is
**never committed** (it's in `.gitignore`). Use it to validate our protocol behavior.

## Extract & run

```bash
# the tarball ships with the subject; adjust the path to where you saved it
tar xzf /path/to/zappy_ref-v3.0.1.tgz -C reference/

# run the reference server
./reference/zappy_server -p 4242 -x 10 -y 10 -n red blue -c 4 -f 100

# compare our GUI / AI against it
./zappy_gui -p 4242
./zappy_ai  -p 4242 -n red
```

Use the reference to settle any protocol ambiguity: **its behavior is authoritative**
over our reading of the PDF. If they disagree, open an ADR.
