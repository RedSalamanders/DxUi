- RichTextEditor file paste now generates displayed Markdown images for recognized
  image suffixes and full-filename Markdown links for other absolute Unicode file
  paths. Mixed Explorer lists preserve order, relative/file-URI destinations and
  one undo boundary. Source/code contexts keep literal paths; paste never opens,
  copies or moves the files. Native and embedded clipboard tests cover mixed
  references, exact source/semantic roles, undo and allocation-failure recovery.
