After `M-x customize-variable RET LaTeX-font-list RET`:

You should see that most of the `Key` field defined by AUCTeX is invisible, this
is because the key here are shown **as it is**, so things like `C-a` are
invisible. So, you have to **insert the non-graphical char (such as control-c)**
yourself in the `customize` buffer. Say you wanna bind `C-c C-f C-x` to
inserting `aaa[your text]bbb` then try:

 1. Press the `INS` button to insert a new entry
 2. In the `Prefix` field, type： `aaa`
 3. In the `Suffix` field, type： `bbb`
 4. In the `Key` field, type : `C-q C-x` (this will insert a literal `C-x` into
    the buffer, `C-q` stands for `(quoted-insert)`)
 5. Click `Apply And Save` at the top.
 
You cannot fill emacs-lisp expression in the these fields.

In fact it's easier (and safer) to set `Key` field to something like `x`, this
will bind your `PREFIX(*)SUFFIX` pair to key `C-c C-f x`.
