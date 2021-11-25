# -*- coding: utf-8 -*-

from natasha import (
    Segmenter,
    
    NewsEmbedding,
    NewsMorphTagger,
    
    Doc
)

segmenter = Segmenter()
emb = NewsEmbedding()
morph_tagger = NewsMorphTagger(emb)
from natasha import MorphVocab
morph_vocab = MorphVocab()

import sys
from os import path
from time import time

import struct
import ctypes

def notify(current_sec, message, delay = 10):
    current_time = time()

    if (current_time - current_sec) > delay:
        print(message)
        current_sec = current_time
    
    return current_sec

class posting_list:

    def __init__(self, docs_id=[], starts=[], ends=[]):
        self.docs_id = docs_id
        self.starts = starts
        self.ends = ends

def get_terms(document):
    doc = Doc(document)
    doc.segment(segmenter)

    doc.tag_morph(morph_tagger)

    for token in doc.tokens:
        token.lemmatize(morph_vocab)
    
    terms = {}
    
    for token in doc.tokens:

        fail = False
        for letter in token.lemma:
            if not (letter.isalpha() or letter.isdigit() or letter == '-'):
                fail = True
                break
        
        if fail:
            continue

        try: # проверка ошибок кодирования в UTF-16
            ctypes.create_unicode_buffer(token.lemma, size=len(token.lemma))
        except:
            continue

        if token.lemma not in terms:
            terms[token.lemma] = [(token.start, token.stop)]
        else:
            terms[token.lemma].append((token.start, token.stop))
    return terms

def store_terms(doc_id, terms, tree):
    if len(terms) == 0:
        return

    for term, coords in terms.items():
        starts = []
        ends = []

        for start, end in coords:
            starts.append(start)
            ends.append(end)
        
        if term not in tree:
            tree[term] = posting_list([doc_id], [starts], [ends])
        else:
            tree[term].docs_id.append(doc_id)
            tree[term].starts.append(starts)
            tree[term].ends.append(ends)

def main():

    if len(sys.argv) < 6 + 1:
        print("Error")
        return
    
    path2block = False
    path2index_dir = False
    block_id = -1

    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "-i":
            path2block = sys.argv[i + 1]
            i += 1
        elif arg == "-o":
            path2index_dir = sys.argv[i + 1]
            i += 1
        elif arg == "-id":
            block_id = int(sys.argv[i + 1])
            i += 1
        
        i += 1
    
    if not path2block or not path2index_dir or not (block_id + 1):
        print("Error in argv")
        return

    buf = ""
    offsets = []
    offset = 0
    n_docs = 0
    tree = {}

    with open(path2block, "r", encoding="UTF-8") as fp:
        current_sec = time()

        for buf in fp:
            size = len(buf.encode(encoding="UTF-8"))
            offsets.append(offset)
            offset += size + 1

            terms = get_terms(buf)
            store_terms(n_docs, terms, tree)
            n_docs += 1

            current_sec = notify(current_sec, 
                                 "{:d} docs were processed in block {:d}".format(n_docs, block_id + 1),
                                 60)

    offsets.append(offset)

    # Заполнение файла с таблицей документов docs_id
    with open(path.join(path2index_dir, "block_docs_id_{:d}.data".format(block_id + 1)), "wb") as fp:
        fp.write(struct.pack("I", n_docs))

        n_chars = len(path2block)
        offset = 0

        j = 0
        while j < n_docs + 1:
            fp.write(struct.pack("I", offset))
            offset += ctypes.sizeof(ctypes.c_uint) + n_chars * ctypes.sizeof(ctypes.c_wchar) + 2 * ctypes.sizeof(ctypes.c_uint)
            j += 1

        j = 0
        while j < n_docs:
            fp.write(struct.pack("I", n_chars))
            fp.write(ctypes.create_unicode_buffer(path2block, n_chars))
            fp.write(struct.pack("I", offsets[j]))
            size = offsets[j + 1] - offsets[j]
            fp.write(struct.pack("I", size))
            j += 1

    # Заполнение файла с термами, словопозициями и координатами postings_list и terms
    n_terms = len(tree)
    fp_terms = open(path.join(path2index_dir, "block_terms_{:d}.data".format(block_id + 1)), "wb")
    fp_postings_list = open(path.join(path2index_dir, "block_postings_list_{:d}.data".format(block_id + 1)), "wb")

    fp_terms.write(struct.pack("I", n_terms))
    fp_postings_list.write(struct.pack("I", n_terms))

    print("Block {:d} has {:d} terms and {:d} docs".format(block_id + 1, n_terms, n_docs))

    offset = 0
    data = [(key, value) for key, value in tree.items()]
    data.sort(key = lambda x: x[0])

    for term, posting in data:
        n_chars = len(term)
        fp_terms.write(struct.pack("I", n_chars))
        fp_terms.write(ctypes.create_unicode_buffer(term, n_chars)) # пишем терм
        fp_terms.write(struct.pack("I", offset))

        n_docs = len(posting.docs_id)
        fp_postings_list.write(struct.pack("I", n_docs))
        fp_postings_list.write(struct.pack(str(n_docs) + "i", *posting.docs_id)) # список идентификаторов

        freq = [0] * n_docs
        offsets = [0] * (n_docs + 1)
        
        for j in range(n_docs):
            freq[j] = len(posting.starts[j])
            offsets[j + 1] = offsets[j] + 2 * freq[j]

        fp_postings_list.write(struct.pack(str(n_docs) + "i", *freq))    # список частот
        fp_postings_list.write(struct.pack(str(n_docs) + "I", *offsets[:-1])) # смещения до координат

        for j in range(n_docs):
            fp_postings_list.write(struct.pack(str(freq[j]) + "i", *(posting.starts[j]))) # координаты начал
            fp_postings_list.write(struct.pack(str(freq[j]) + "i", *(posting.ends[j])))   # координаты концов

        offset += ctypes.sizeof(ctypes.c_uint) +               \
                  2 * n_docs * ctypes.sizeof(ctypes.c_int) +   \
                  n_docs * ctypes.sizeof(ctypes.c_uint) +      \
                  offsets[-1] * ctypes.sizeof(ctypes.c_int)

    fp_terms.close()
    fp_postings_list.close()
   
if __name__== "__main__":
    main()

