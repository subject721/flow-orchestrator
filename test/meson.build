

test_file_names += 'test_flow01.lua'


foreach test_file_name : test_file_names

    test_file_full = test_file_name

    test_file_targets += configure_file(input : files(test_file_full),
                output : test_file_full,
                copy : true)

endforeach