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

def is_term(token):
    return token not in {"(", ")", "\"", "!", "/", "&", "|"}

def main():

    if len(sys.argv) < 3:
        print("Error")
        return
    
    path2in_file = sys.argv[1]
    path2out_file = sys.argv[2]
    
    text = ""
    with open(path2in_file, "r", encoding="Windows 1251") as fp:
        text = fp.read()
    
    doc = Doc(text)
    
    doc.segment(segmenter)
    doc.tag_morph(morph_tagger)
    
    for token in doc.tokens:
        token.lemmatize(morph_vocab)
    
    request = []
    
    tokens = doc.tokens

    i = 0
    while i < len(tokens):

        if i + 1 < len(tokens):
            if tokens[i].lemma == "&" and  tokens[i+1].lemma == "&":
                request.append("&")
                i += 2
                continue
            elif tokens[i].lemma == "|" and tokens[i+1].lemma == "|":
                request.append("|")
                i += 2
                continue

        if tokens[i].lemma in {"(", ")", "!", "/", "\""}:
            request.append(tokens[i].lemma)
        else:
            fail = False
            for letter in tokens[i].lemma:
                if not (letter.isalpha() or letter.isdigit() or letter == '-'):
                    fail = True
                    break
            
            if not fail:
                request.append(tokens[i].lemma)
            
        i += 1
    
    fuzzy = False
    if {"(", ")", "!", "/", "\"", "&", "|"} & set(request) == set(): # проверка на начёткий поиск
        fuzzy = True

    i = 0
    begin = False

    while (not fuzzy) and i < len(request) - 1:
        cur = request[i]

        if begin and cur == "\"":
            begin = False
            i += 1
            continue

        if not begin and cur == "\"":
            begin = True
            i += 1
            continue

        if begin:
            i += 1
            continue

        next = request[i+1]

        if is_term(cur) and is_term(next) or \
           is_term(cur) and next == "!"   or \
           is_term(cur) and next == "("   or \
           is_term(cur) and next == "\""  or \
           cur == "\"" and is_term(next)  or \
           cur == ")" and next == "("     or \
           cur == ")" and is_term(next)   or \
           cur == ")" and next == "!":
            request.insert(i + 1, "&")
            i += 1

        i += 1

    with open(path2out_file, "w", encoding="Windows 1251") as fp:
        if fuzzy:
            fp.write("!! ")
        for term in request:
            fp.write(term + " ")
        fp.write("\n")
            
if __name__== "__main__":
    main()
