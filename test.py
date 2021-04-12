import unittest
import os

current_directory = ""


class TestBablib(unittest.TestCase):

    @classmethod
    def setUp(self):
        os.chdir(current_directory + '/tmp')

    @staticmethod
    def get_ls_output(d=''):
        stream = os.popen(f'ls {d}')
        output = stream.read()
        actual = set(output.split())
        stream.close()
        return actual

    def test_structure(self):
        # 2 rooms, 4 bookcases and desk in root room of the library
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_9', 'room_1', 'desk'})

        # 1 room, 4 bookcases and desk in the room, which we came to from another room
        os.chdir('room_1')
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_2', 'desk'})

        # 5 shelves in bookcase
        os.chdir('bookcase_2')
        self.assertSetEqual(self.get_ls_output(), {'shelf_0', 'shelf_1', 'shelf_2', 'shelf_3', 'shelf_4'})


if __name__ == '__main__':
    current_directory = os.getcwd()
    unittest.main()
