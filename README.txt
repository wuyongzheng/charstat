charstat: character statistics.
It counts the occurrence of each character.
It works on either byte level (default) or UTF-8 character level.

uniqc: count duplicated lines in a text file.
It's similar to `sort | uniq -c`, but more efficient.

ngram: output all N-grams of a text file.
It only works on UTF-8.
All characters except the new line characters (\r and \n) are included in N-grams.
