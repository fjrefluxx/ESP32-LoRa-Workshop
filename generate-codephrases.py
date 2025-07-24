import math

def generate_output():
    '''
    Generate the different mappings for device IDs, personalized keys, and passphrases.
    '''
    
    # define the device IDs
    device_IDs = [
        0x11, 0x22, 0x33, 0x44, 
        0x55, 0x66, 0x88, 0x99, 
        0xAA, 0xBB, 0xCC, 0xDD, 
        0xEE, 0xF0
    ]

    # define mapping for a KEY to individual PASSPHRASES 
    keymap = [
        ("zbgj5F", "Your passphrase: Hot Potato"), 
        ("A7Fwx4", "Your passphrase: Minions"), 
        ("GiZ58h", "Your passphrase: Factorio"),
        ("Qqk8Uq", "Your passphrase: Macaroni"),
        ("rx9rGN", "Your passphrase: Sushi"),
        ("Loe7hJ", "Your passphrase: Smoking is BAD"),
        ("9VT6qw", "Your passphrase: Paint3D"),
        ("tsa5PB", "Your passphrase: Cold Feet"),
        ("Dj8wWQ", "Your passphrase: Couch Potato"),
        ("5Ad6d5", "Your passphrase: Spill the Beans"),
        ("o8bZPE", "Your passphrase: ESP32-v5"),
        ("ST2qps", "Your passphrase: Piece of Cake"),
        ("oP6URu", "Your passphrase: Bread Crumbs"),
        ("7wHYvR", "Your passphrase: Cocktail Bar")
    ]

    print("Copy this block for the plaintext mapping: (Level 4)")
    print_plaintextmap(keymap)
    print("")
    print("Copy this block for the codetext mapping: (Level 4)")
    print_codemap(encode(keymap))
    print("")
    print("Copy this block for the deviceID-groupkey mapping: (Level 3+4)")
    print_groupkey_map(keymap, device_IDs)
###

def encode(keymap):
    '''
    Encode the passphrases from the given keymap with the respective keys by performing a simple bit-wise XOR operation.
    Outputs the decrypted hex repesentation as a list of formatted strings to be used in the c++ code.
    '''

    encodemap = []

    for i in  range(len(keymap)):
        text = keymap[i][1]
        key = padded_key(text, keymap[i][0])

        a = int(text_to_bits(text), 2)
        b = int(text_to_bits(key), 2)

        xor = a ^ b
        hexrep = hex(xor)

        n = 2
        s = [hexrep[i:i+n] for i in range(0, len(hexrep), n)]
        s = s[1:]
        all : str = "{"

        for e in s:
            all += ("0x" + e + ", ")

        all = all[:-2]
        all += "}"

        xor2 = xor ^ b
        encodemap.append((keymap[i][0], all))
    return encodemap

##################
## conversions
##################

def padded_key(text:str, key:str) -> str:
    upfilled = ""
    n = math.ceil(len(text)/len(key))
    for i in range(n):
        upfilled += key
    return upfilled[:len(text)]


def text_to_bits(text, encoding='utf-8', errors='surrogatepass'):
    bits = bin(int.from_bytes(text.encode(encoding, errors), 'big'))[2:]
    return bits.zfill(8 * ((len(bits) + 7) // 8))


def text_from_bits(bits, encoding='utf-8', errors='surrogatepass'):
    n = int(bits, 2)
    return n.to_bytes((n.bit_length() + 7) // 8, 'big').decode(encoding, errors) or '\0'


def text_from_realbits(realbits, encoding='utf-8', errors='surrogatepass'):
    return realbits.to_bytes((realbits.bit_length() + 7) // 8, 'big').decode(encoding, errors) or '\0'

####################################
## print functions
####################################

def print_codemap(kvmap):
    print("std::map<String, std::vector<unsigned char>> codetextmap {")
    for i in range(len(kvmap)):
        key = kvmap[i][0]
        text = kvmap[i][1]        
        print('  {\"' + key + '\", ' + text + '},')
    print("};")


def print_plaintextmap(kvmap):
    print("std::map<String, String> plaintextmap {")
    for i in range(len(kvmap)):
        key = kvmap[i][0]
        text = kvmap[i][1]        
        print('  {\"' + key + '\", "' + text + '\"},')
    print("};")


def print_groupkey_map(kvmap, deviceID_list):
    if len(kvmap) > len(deviceID_list):
        print("Error: deviceID list is shorter than the given group key list")
        return

    print("std::map<byte, String> groupkeys{")
    for i in range(len(kvmap)):
        syncword = hex(deviceID_list[i])
        key = kvmap[i][0]        
        print('  {' + syncword + ', "' + key + '\"},')
    print("};")




if __name__ == '__main__':    
    generate_output()
    