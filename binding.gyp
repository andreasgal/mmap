{
  "targets": [
    {
      "target_name": "mmap",
      "sources": [
        "mmap.cpp",
      ],
      "include_dirs": [
        '<!(node -e "require(\'nan\')")',
      ],
    }
  ]
}
