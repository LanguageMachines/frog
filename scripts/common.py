import unicodedata


__all__ = ["ID", "FORM", "LEMMA", "CPOSTAG", "POSTAG",
		   "FEATS", "HEAD", "DEPREL", "PHEAD", "PDEPREL"]

ID, FORM, LEMMA, CPOSTAG, POSTAG, \
	FEATS, HEAD, DEPREL, PHEAD, PDEPREL = range(10)


def isScoringToken(token):
	for chr in token.decode("utf-8"):
		if unicodedata.category(chr) == "Po":
			return False

	return True


def pairIterator(sentence, options):
	for dependent in sentence:
		for head in sentence:
			if dependent is not head:
				if not options.skipNonScoring or \
					   isScoringToken(dependent[FORM]):
					dist = abs(int(dependent[ID]) - int(head[ID]))
					if not options.maxDist or dist <= options.maxDist:
						yield dependent, head
