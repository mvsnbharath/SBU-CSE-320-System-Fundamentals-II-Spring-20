--- hw1/dev_repo/solution/src/sequitur.c

+++ hw1/repos/vmeadam/hw1/src/sequitur.c

@@ -51,7 +51,7 @@

 	//    abbbc  ==> abbc   (handle this first)

 	//      ^

 	//    abbbc  ==> abbc   (then check for this one)

-        //     ^

+        //     ^ 

 	if(next->prev && next->next &&

 	   next->value == next->prev->value && next->value == next->next->value)

 	    digram_put(next);

@@ -125,7 +125,7 @@

     // We are destroying any digram that starts at this symbol,

     // so we have to delete that from the table.

     digram_delete(this);

-

+    

     // Splice the body of the rule in place of the nonterminal symbol.

     join_symbols(left, first);

     join_symbols(last, right);

@@ -188,7 +188,7 @@

 static void process_match(SYMBOL *this, SYMBOL *match) {

     debug("Process matching digrams <%lu> and <%lu>",

 	  SYMBOL_INDEX(this), SYMBOL_INDEX(match));

-    SYMBOL *rule = NULL;

+    SYMBOL *rule = NULL; 

 

     if(IS_RULE_HEAD(match->prev) && IS_RULE_HEAD(match->next->next)) {

 	// If the digram headed by match constitutes the entire right-hand side
