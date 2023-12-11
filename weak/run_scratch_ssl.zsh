S_GREEN="\x1b[32m"
S_NOR="\x1b[0m"

tmp=".tmp"
pj=peer.json
ns=(ca n0 n1 n2)
sk=secret-key.pem
pk=public-key.pem
sg=cert.sig
typeset -A ap
ap=(7777 n0 7778 n1 7779 n2)
w=./build-weak/wch
ports=(${(k)ap})

# 0. prepare the keys
# --------------------------------------------------
# 0.1 for a 3-nodes cluster, generated 4 key pairs (1 ca + 3 pks)

rm $tmp -rf
mkdir $tmp
cd $tmp
for n in $ns; do
    # generate the secret key
    openssl genpkey -algorithm ed25519 -out $n-$sk
    # extract the public key
    openssl pkey -in $n-$sk -pubout -out $n-$pk
    print "🌱 Genereted " $S_GREEN $n-$sk $n-$pk $S_NOR
done

# 0.2 for each of the nodes, issue the certificate
for n in ${ns[2,-1]}; do            # for each nodes
    openssl pkeyutl -sign -rawin -in $n-$pk -inkey ${ns[1]}-$sk -out $n-$sg
    print "\t⚙️ Generated" $S_GREEN $n-$sg $S_NOR
done

# 0.3 write the peer json
echo '{' > $pj
for port in ${ports}; do
    v=${ap[$port]}
    echo \" localhost:$port \" \: \{ \
         \"pk_pem_file\"\: \"$tmp/$v-$pk\" ',' \
         \"cert_file\"\: \"$tmp/$v-$sg\" \
         \} >> $pj

    if ! [[ $port = ${ports[-1]} ]]; then
        echo ',' >> $pj
    fi
done
echo '}' >> $pj

print 'Generated'  $S_GREEN $(ls $pj) $S_NOR
cd ..

# 1. try start the cluster
# --------------------------------------------------
ops=(--mock-exe --without-crypto no \
                --crypto.ca_public_key_pem_file $tmp/ca-$pk \
                --crypto.peer_json_file_or_string $tmp/$pj \
    )
# the primary n0
n=${ap[${ports[1]}]}
$w --port ${ports[1]} \
   --crypto.node_secret_key_pem_file $tmp/$n-$sk \
   --crypto.node_cert_file $tmp/$n-$sg \
   $ops

# the n1
n=${ap[${ports[2]}]}
$w --port ${ports[2]} \
   --Solo.node-to-connect localhost:${ports[1]} \
   --crypto.node_secret_key_pem_file $tmp/$n-$sk \
   --crypto.node_cert_file $tmp/$n-$sg \
   $ops
